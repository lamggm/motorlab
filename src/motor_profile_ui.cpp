#include "window.h"
#include "motor_profiles.h"
#include "config_text.h"
#include "clickedit.h"
#include <QDialog>
#include <QFile>
#include <memory>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QVBoxLayout>

QJsonObject Window::captureMotorProfile(int channel, const QString &name, QString *error) const {
  QJsonArray values;
  for (const auto &p : parameters)
    values.append(QJsonObject{{"query", "~"+p.suffix}, {"raw", double(p.raw)},
                              {"readable", p.known}, {"draft", p.dirty}});
  return MotorProfiles::capture(values, channel, name,
      session.connected()?session.model():offlineModel->currentText(),
      session.connected()?session.firmware():profileFirmware,
      session.connected()?(session.isDemo()?"demo":"serial"):profileSource, error);
}

bool Window::stageMotorProfile(const QJsonObject &profile, int channel, bool calibration, QString *error) {
  if (error) error->clear();
  if (applying || readsPending || session.uploading() || session.streaming() || manualDriveActive()) {
    if(error)*error="Encerre a operação ou o controle manual antes de preparar um perfil.";
    return false;
  }
  const auto plan=MotorProfiles::plan(profile, channel, calibration,
      session.connected()?session.model():offlineModel->currentText(), session.firmware(),
      session.connected()&&!session.isDemo(), error);
  if(plan.isEmpty())return false;
  QVector<QPair<int,qint64>> staged;
  QJsonArray changes;
  for(auto v:plan) {
    const auto e=v.toObject(); int row=-1;
    for(int i=0;i<parameters.size();++i)
      if("~"+parameters[i].suffix==e["query"].toString()) {row=i;break;}
    if(row<0 || !parameters[row].editor) {
      if(error)*error="Parâmetro indisponível na interface: "+e["query"].toString();
      return false;
    }
    if(session.connected() && !parameters[row].known) {
      if(error)*error="Leia a configuração do controlador antes de preparar o perfil: "+e["query"].toString();
      return false;
    }
    staged.append({row,qint64(e["raw"].toDouble())});
    changes.append(QJsonObject{{"query",e["query"]},{"raw",e["raw"]}});
  }
  if(session.isRemote()) {
    const auto reply=session.backendCall("motorlab_stage",{{"changes",changes}});
    if(!reply["ok"].toBool()) {if(error)*error=reply["error"].toString();return false;}
  }
  // All values and mappings are validated before touching a draft. No serial
  // write is made here; the shared backend owns Apply RAM / EEPROM separately.
  for(auto value:staged) {
    auto &p=parameters[value.first]; QSignalBlocker block(p.editor);
    p.editor->setValue(value.second/p.scale);
    p.dirty=!p.known || p.raw!=value.second;
    parameterItems[value.first]->setText(0,p.name+" · "+p.suffix+(p.dirty?" *":""));
  }
  profileModel=profile["model"].toString(); profileFirmware=profile["firmware"].toString();
  profileSource=profile["source"].toString(); refreshState();
  return true;
}

void Window::showMotorProfiles() {
  if(auto existing=findChild<QDialog *>("motorProfilesDialog")) {
    if(existing->isVisible()) {existing->raise();return;}
    existing->setObjectName("motorProfilesClosedDialog");
  }
  auto dialog=new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setObjectName("motorProfilesDialog"); dialog->setWindowTitle("Perfis de motor");
  dialog->resize(980,720);
  auto layout=new QVBoxLayout(dialog);
  auto title=new QLabel("Biblioteca de motores",dialog);title->setObjectName("pageTitle");layout->addWidget(title);
  auto intro=new QLabel("Escolha o motor e o canal. Com o driver conectado, leia sua configuração primeiro. Preparar cria rascunhos para revisão. Depois use Aplicar alterações (RAM) e, se desejar conservar, Salvar permanente (EEPROM).",dialog);
  intro->setWordWrap(true); layout->addWidget(intro);
  auto row=new QHBoxLayout;
  auto select=new ClickEdit<QComboBox>(dialog);select->setObjectName("motorProfileSelection");
  select->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);select->setMinimumContentsLength(32);
  row->addWidget(select,1);
  auto channel=new ClickEdit<QComboBox>(dialog);channel->setObjectName("motorProfileChannel");
  channel->addItems({"Motor 1 · encoder A1/B1","Motor 2 · encoder A2/B2"});row->addWidget(channel);
  layout->addLayout(row);
  auto description=new QLabel(dialog);description->setObjectName("motorProfileDescription");description->setWordWrap(true);layout->addWidget(description);
  auto calibration=new QCheckBox("Incluir calibração física de Hall / SinCos",dialog);
  calibration->setObjectName("motorProfileCalibration");
  calibration->setToolTip("Use somente para o mesmo conjunto físico de motor e sensores já calibrado. A calibração depende do alinhamento e da montagem.");layout->addWidget(calibration);
  auto table=new QTableWidget(0,4,dialog);table->setObjectName("motorProfilePreview");
  table->setHorizontalHeaderLabels({"Parâmetro do canal escolhido","Atual","Perfil","Unidade"});
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->hide();table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
  table->setColumnWidth(1,110);table->setColumnWidth(2,145);table->setColumnWidth(3,115);layout->addWidget(table,1);
  auto info=new QLabel(dialog);info->setObjectName("motorProfileNotice");info->setWordWrap(true);layout->addWidget(info);
  auto files=new QHBoxLayout;
  auto read=new QPushButton("Ler controlador…",dialog);read->setObjectName("motorProfileRead");
  read->setEnabled(session.connected()&&!applying&&!readsPending&&!session.streaming());
  files->addWidget(read);
  connect(read,&QPushButton::clicked,dialog,[=] {dialog->close();readParameters();});
  auto capture=new QPushButton("Salvar este canal como perfil…",dialog);capture->setObjectName("motorProfileCapture");
  auto import=new QPushButton("Importar…",dialog); auto exportFile=new QPushButton("Exportar…",dialog);
  auto remove=new QPushButton("Excluir pessoal",dialog);remove->setObjectName("motorProfileRemove");
  files->addWidget(capture);files->addWidget(import);files->addWidget(exportFile);files->addWidget(remove);layout->addLayout(files);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);
  auto prepare=buttons->addButton("Preparar alterações",QDialogButtonBox::ActionRole);prepare->setObjectName("motorProfilePrepare");
  prepare->setDefault(false);buttons->button(QDialogButtonBox::Close)->setText("Fechar");
  buttons->button(QDialogButtonBox::Close)->setDefault(true);
  layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);
  auto profiles=std::make_shared<QJsonArray>();
  auto preview=[=] {
    auto p=(select->currentIndex()>=0?profiles->at(select->currentIndex()).toObject():QJsonObject());
    const bool hasCalibration=!p.contains("calibration_available")||p["calibration_available"].toBool();
    if(!hasCalibration){QSignalBlocker block(calibration);calibration->setChecked(false);}
    calibration->setEnabled(hasCalibration&&!p.isEmpty());
    description->setText(p["description"].toString("Perfil pessoal salvo da configuração lida do canal.")+"\n"+
      "Controlador: "+p["model"].toString()+" · "+p["firmware"].toString()+"\nRegistro: "+p["timestamp_utc"].toString());
    description->setToolTip("Origem: "+p["provenance"].toObject()["file"].toString());
    remove->setEnabled(!p.isEmpty()&&!p["builtin"].toBool()); exportFile->setEnabled(!p.isEmpty());
    QString error;
    // Preview can be examined even when a connected firmware prevents staging.
    auto plan=MotorProfiles::plan(p,channel->currentIndex()+1,calibration->isChecked(),p["model"].toString(),p["firmware"].toString(),false,&error);
    table->setRowCount(plan.size());
    int mode=-1;
    for(auto e:plan) if(e.toObject()["query"].toString()=="~MMOD "+QString::number(channel->currentIndex()+1))mode=e.toObject()["raw"].toInt();
    for(int r=0;r<plan.size();++r) {
      const auto e=plan[r].toObject();const auto suffix=e["query"].toString().mid(1);
      for(const auto &param:parameters) if(param.suffix==suffix) {
        auto name=new QTableWidgetItem(ConfigText::name(*param.spec,param.channel)+" · "+suffix);
        name->setToolTip(ConfigText::tooltip(*param.spec,param.channel));table->setItem(r,0,name);
        table->setItem(r,1,new QTableWidgetItem(param.known?QString::number(param.raw/param.scale,'g',9)+" "+param.unit:"—"));
        auto original=Catalog::commonOriginal(param.key,param.channel);QString unit=param.spec->indexUnits.value(QString::number(param.channel)).toString(param.spec->unit);
        if(param.key=="FEW") {if(mode==1)unit="RPM";else if(mode==3||mode==4)unit="contagens";}
        if((param.key=="MAC"||param.key=="MDEC"||param.key=="EDEC")&&mode==5)unit="mA/s";
        QString value=QString::number(e["raw"].toDouble()/param.scale,'g',9);
        for(auto choice:original["choices"].toArray()) {
          auto c=choice.toObject();if(c["value"].toDouble()==e["raw"].toDouble())value=ConfigText::choice(c["label"].toString());
        }
        table->setItem(r,2,new QTableWidgetItem(value));table->setItem(r,3,new QTableWidgetItem(unit));break;
      }
    }
    auto checked=MotorProfiles::plan(p,channel->currentIndex()+1,calibration->isChecked(),
      session.connected()?session.model():offlineModel->currentText(),session.firmware(),session.connected()&&!session.isDemo(),&error);
    if(session.connected()&&!checked.isEmpty()) {
      for(auto e:checked) for(const auto &param:parameters)
        if("~"+param.suffix==e.toObject()["query"].toString()&&!param.known) {error="Leia a configuração do controlador antes de preparar o perfil.";break;}
    }
    prepare->setEnabled(!checked.isEmpty()&&error.isEmpty());
    info->setText(error.isEmpty()?QString("%1 ajustes do Motor %2. Configurações gerais e o outro motor são preservados. Rascunhos destes campos serão substituídos. %3")
      .arg(plan.size()).arg(channel->currentIndex()+1).arg(calibration->isChecked()?"Calibração física incluída.":"Calibração física excluída."):error);
  };
  auto reload=[=] {
    const int previous=select->currentIndex();*profiles=MotorProfiles::library();QSignalBlocker block(select);select->clear();
    for(auto v:*profiles)select->addItem(v.toObject()["name"].toString());
    select->setCurrentIndex(qBound(0,previous,select->count()-1));preview();
  };
  connect(select,&QComboBox::currentIndexChanged,dialog,[=]{QSignalBlocker block(calibration);calibration->setChecked(false);preview();});
  connect(channel,&QComboBox::currentIndexChanged,dialog,[=]{preview();});
  connect(calibration,&QCheckBox::toggled,dialog,[=]{preview();});
  connect(prepare,&QPushButton::clicked,dialog,[=] {
    QString error;
    if(!stageMotorProfile((select->currentIndex()>=0?profiles->at(select->currentIndex()).toObject():QJsonObject()),channel->currentIndex()+1,calibration->isChecked(),&error)) {info->setText(error);return;}
    notify(QString("Perfil preparado para Motor %1. Revise os rascunhos; use Aplicar alterações (RAM) para enviar.").arg(channel->currentIndex()+1));
    dialog->accept();
  });
  connect(capture,&QPushButton::clicked,dialog,[=] {
    if(applying||readsPending||session.uploading()) {info->setText("Aguarde a operação atual antes de salvar.");return;}
    bool ok=false;const auto name=QInputDialog::getText(dialog,"Salvar perfil de motor","Nome do motor e do ajuste",QLineEdit::Normal,QString(),&ok);if(!ok)return;
    QString error;auto p=captureMotorProfile(channel->currentIndex()+1,name,&error);
    if(p.isEmpty()||!MotorProfiles::save(p,&error)) {info->setText(error);return;}
    reload();
    for(int i=0;i<profiles->size();++i) {const auto candidate=profiles->at(i).toObject();
      if(candidate["name"]==p["name"]&&candidate["timestamp_utc"]==p["timestamp_utc"]) {select->setCurrentIndex(i);break;}}
    info->setText("Perfil salvo na biblioteca do computador. RAM e EEPROM não foram alteradas.");
  });
  connect(import,&QPushButton::clicked,dialog,[=] {
    auto path=QFileDialog::getOpenFileName(dialog,"Importar perfil de motor",QString(),"Perfil de motor (*.json)");if(path.isEmpty())return;
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024){info->setText("Arquivo inacessível ou maior que 1 MiB.");return;}
    QString error;auto p=QJsonDocument::fromJson(f.readAll()).object();
    if(!MotorProfiles::save(p,&error)) {info->setText(error);return;}reload();info->setText("Perfil importado para a biblioteca.");
  });
  connect(exportFile,&QPushButton::clicked,dialog,[=] {
    auto path=QFileDialog::getSaveFileName(dialog,"Exportar perfil de motor","perfil-motor.json","Perfil de motor (*.json)");if(path.isEmpty())return;
    auto bytes=QJsonDocument((select->currentIndex()>=0?profiles->at(select->currentIndex()).toObject():QJsonObject())).toJson();QSaveFile f(path);
    info->setText(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit()?"Perfil exportado.":"Falha ao exportar perfil.");
  });
  connect(remove,&QPushButton::clicked,dialog,[=] {
    auto p=(select->currentIndex()>=0?profiles->at(select->currentIndex()).toObject():QJsonObject());if(p["builtin"].toBool())return;
    if(QMessageBox::question(dialog,"Excluir perfil pessoal","Excluir “"+p["name"].toString()+"” da biblioteca?")!=QMessageBox::Yes)return;
    QString error;if(!MotorProfiles::remove(p["id"].toString(),&error)){info->setText(error);return;}reload();
  });
  reload();dialog->show();
}
