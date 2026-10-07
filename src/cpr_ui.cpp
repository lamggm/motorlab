#include "cpr.h"
#include "window.h"
#include <QDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QVBoxLayout>

static CprProfile readCpr(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) {
    CprProfile p;
    p.error = "CPR inacessível ou maior que 1 MiB.";
    return p;
  }
  return CprProfile::parse(f.readAll());
}
bool Window::saveCpr(const QString &path) {
  if (applying || readsPending)
    return false;
  const QString model = session.connected()
                            ? Protocol::supportedModel(session.model())
                            : offlineModel->currentText();
  const QString firmwareVersion =
      session.connected() ? session.firmware() : profileFirmware;
  if (!firmwareVersion.isEmpty() &&
      !QRegularExpression("v3\\.0[a-z]?\\b").match(firmwareVersion).hasMatch())
    return false;
  QFile metadata(":/original-trees.json");
  if (!metadata.open(QIODevice::ReadOnly))
    return false;
  auto tree =
      QJsonDocument::fromJson(metadata.readAll()).object()[model].toObject();
  CprProfile profile;
  profile.firmware = "v3.0:" + model;
  profile.roborun = "MotorLab Linux 0.6.0 · perfil parcial";
  if (session.isDemo() || profileSource == "demo")
    profile.roborun += " · DEMO";
  profile.controlTree = tree["controlTree"].toString();
  profile.powerTree = tree["powerTree"].toString();
  QSet<QString> seen;
  for (auto value : tree["entries"].toArray()) {
    auto entry = value.toObject();
    const auto key = entry["command"].toString();
    int index = entry["index"].toInt();
    for (const auto &p : parameters) {
      if (p.key != key || (p.channel ? p.channel : 1) != index ||
          (!p.known && !p.dirty))
        continue;
      CprItem item;
      item.command = key;
      item.index = index;
      if (seen.contains(item.identity()))
        break;
      if (entry["type"].toString() == "String")
        break;
      item.tree = entry["tree"].toString();
      item.type = entry["type"].toString();
      QStringList nodes;
      for (auto n : entry["path"].toArray())
        nodes.append(n.toString());
      item.node = nodes.join('|');
      item.multiplier = qRound(p.scale);
      item.value = QString::number(
          p.dirty && p.editor ? qRound64(p.editor->value() * p.scale) : p.raw);
      profile.items.append(item);
      seen.insert(item.identity());
      break;
    }
  }
  if (profile.items.isEmpty())
    return false;
  const auto data = profile.xml();
  QSaveFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() &&
         f.commit();
}
void Window::showCpr(bool compare, const QString &inputPath) {
  const auto path =
      inputPath.isEmpty()
          ? QFileDialog::getOpenFileName(this, "Abrir perfil RoboRun",
                                         QString(), "RoboRun (*.cpr)")
          : inputPath;
  if (path.isEmpty())
    return;
  const auto profile = readCpr(path);
  if (!profile.valid()) {
    notify(profile.error);
    return;
  }
  CprProfile other;
  if (compare) {
    auto second = QFileDialog::getOpenFileName(
        this, "Comparar com outro perfil", QString(), "RoboRun (*.cpr)");
    if (second.isEmpty())
      return;
    other = readCpr(second);
    if (!other.valid()) {
      notify(other.error);
      return;
    }
  }
  auto dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setObjectName("cprDialog");
  dialog->setWindowTitle(compare ? "Comparar perfis CPR"
                                 : "Perfil RoboRun · importar rascunhos");
  dialog->resize(1050, 700);
  auto layout = new QVBoxLayout(dialog);
  auto info = new QLabel(
      profile.firmware + " · " + profile.roborun +
      (compare ? "\nComparado com: " + other.firmware + " · " + other.roborun
               : ""));
  info->setWordWrap(true);
  layout->addWidget(info);
  auto table = new QTableWidget;
  table->setObjectName("cprTable");
  table->verticalHeader()->hide();
  table->setColumnCount(compare ? 5 : 4);
  table->setHorizontalHeaderLabels(
      compare
          ? QStringList{"Parâmetro", "CPR A", "CPR B", "Resultado", "Caminho"}
          : QStringList{"Parâmetro", "Valor bruto", "Importação", "Caminho"});
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(compare ? 4 : 3,
                                                  QHeaderView::Stretch);
  layout->addWidget(table, 1);
  QVector<QPair<int, qint64>> staged;
  int skipped = 0;
  QMap<QString, CprItem> b;
  for (const auto &item : other.items)
    b.insert(item.identity(), item);
  const auto add = [&](const QStringList &cells) {
    int row = table->rowCount();
    table->insertRow(row);
    for (int c = 0; c < cells.size(); ++c)
      table->setItem(row, c, new QTableWidgetItem(cells[c]));
  };
  QSet<QString> processed;
  for (const auto &item : profile.items) {
    if (processed.contains(item.identity()))
      continue;
    processed.insert(item.identity());
    if (compare) {
      bool exists = b.contains(item.identity());
      const auto value = b.take(item.identity());
      const bool same = exists && item.value == value.value &&
                        item.type == value.type &&
                        item.multiplier == value.multiplier;
      add({item.identity(), item.value, exists ? value.value : "—",
           same     ? "Igual"
           : exists ? "Diferente (valor/tipo/escala)"
                    : "Só A",
           item.node});
      continue;
    }
    QString state = "Sem editor compatível";
    if (item.type != "String")
      for (int i = 0; i < parameters.size(); ++i) {
        const auto &p = parameters[i];
        if (p.key != item.command || (p.channel ? p.channel : 1) != item.index)
          continue;
        auto raw = item.value.toLongLong();
        if (p.editor && item.multiplier == qRound(p.scale) &&
            Catalog::validValue(*p.spec, raw, p.channel)) {
          staged.append({i, raw});
          state = "Rascunho";
        } else
          state = "Não importado: limite, escala ou somente leitura";
        break;
      }
    if (state != "Rascunho")
      ++skipped;
    add({item.identity(), item.value, state, item.node});
  }
  if (compare)
    for (auto i = b.cbegin(); i != b.cend(); ++i)
      add({i.key(), "—", i->value, "Só B", i->node});
  if (!compare) {
    auto import = new QPushButton(
        QString("Importar %1 rascunhos · %2 itens não importados")
            .arg(staged.size())
            .arg(skipped));
    import->setObjectName("importCprDrafts");
    layout->addWidget(import);
    connect(import, &QPushButton::clicked, dialog,
            [this, dialog, info, profile, staged] {
              const QString model =
                  session.connected()
                      ? Protocol::supportedModel(session.model())
                      : offlineModel->currentText();
              if (applying || readsPending || session.streaming() ||
                  session.uploading()) {
                info->setText("Aguarde a operação atual.");
                return;
              }
              if (profile.firmware != "v3.0:" + model) {
                info->setText(
                    "Modelo/geração do CPR diferente da seleção. Selecione o "
                    "modelo correspondente antes de importar.");
                return;
              }
              const bool demo = profile.roborun.contains("DEMO");
              if (demo && session.connected() && !session.isDemo()) {
                info->setText(
                    "Perfil simulado não pode ser importado em unidade real.");
                return;
              }
              for (const auto &value : staged) {
                auto &p = parameters[value.first];
                QSignalBlocker block(p.editor);
                p.editor->setValue(value.second / p.scale);
                p.dirty = !p.known || p.raw != value.second;
                parameterItems[value.first]->setText(
                    0, p.name + " · " + p.suffix + (p.dirty ? " *" : ""));
              }
              profileModel = model;
              profileSource = demo ? "demo" : "cpr";
              profileFirmware.clear();
              refreshState();
              notify(QString("%1 rascunhos importados localmente; revise antes "
                             "de aplicar.")
                         .arg(staged.size()));
              dialog->accept();
            });
  }
  dialog->show();
}
