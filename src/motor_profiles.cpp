#include "motor_profiles.h"
#include "catalog.h"
#include "protocol.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace {
void error(QString *out,const QString &text) { if(out)*out=text; }
QString suffix(const MotorProfiles::Binding &b,int channel) {
  return "~"+b.key+" "+QString::number(channel==1?b.first:b.second);
}
QString storage(const QString &given) { return given.isEmpty()?MotorProfiles::directory():given; }
}
const QVector<MotorProfiles::Binding> &MotorProfiles::bindings() {
  static const QVector<Binding> data=[] {
    QVector<Binding> result;
    const QSet<QString> calibration{"HSAT","ZSMC","ZSMA","HSM","BADJ"};
    const QSet<QString> encoders{"EMOD","EPPR","EHL","ELL","EHOME","EHLA","ELLA"};
    for(const auto &s:Catalog::all()) {
      if(!s.editable)continue;
      if(encoders.contains(s.key)) {
        result.append({s.key,"~"+s.key+" 1",1,2,false,true});continue;
      }
      for(int i=1;i<=s.indexMax;++i) {
        auto path=Catalog::commonOriginal(s.key,i)["path"].toArray();
        if(path.isEmpty() || path.first().toString()!="Motor 1")continue;
        path[0]="Motor 2";
        for(int j=1;j<=s.indexMax;++j)
          if(Catalog::commonOriginal(s.key,j)["path"].toArray()==path) {
            result.append({s.key,"~"+s.key+" "+QString::number(i),i,j,calibration.contains(s.key),false});break;
          }
      }
    }
    return result;
  }();
  return data;
}
QJsonObject MotorProfiles::capture(const QJsonArray &values,int channel,const QString &name,
                                  const QString &model,const QString &firmware,const QString &source,QString *err) {
  if(err)err->clear();
  if(channel<1||channel>2||name.trimmed().isEmpty()||name.size()>160||Protocol::supportedModel(model).isEmpty()) {
    error(err,"Nome, canal ou modelo inválido.");return {};
  }
  QMap<QString,QJsonObject> input;
  for(auto v:values) { auto e=v.toObject();if(input.contains(e["query"].toString())){error(err,"Parâmetro duplicado.");return {};}input[e["query"].toString()]=e; }
  QJsonArray entries;
  for(const auto &b:bindings()) {
    const auto e=input.value(suffix(b,channel));
    if(e.isEmpty() || (!e["readable"].toBool()&&!e["draft"].toBool())) {
      error(err,"Leia a configuração completa do canal antes de salvar: "+suffix(b,channel));return {};
    }
    if(e["draft"].toBool()) {error(err,"Aplique ou descarte os rascunhos deste canal antes de salvar um perfil.");return {};}
    const auto d=e["raw"].toDouble();const auto spec=Catalog::find(b.key);
    if(!e["raw"].isDouble()||!std::isfinite(d)||std::floor(d)!=d||d<spec->minimum||d>spec->maximum||!Catalog::validValue(*spec,qint64(d),channel==1?b.first:b.second)) {
      error(err,"Valor inválido em "+suffix(b,channel));return {};
    }
    qint64 raw=qint64(d);
    if(b.key=="EMOD") {
      if(raw && ((raw&~0x3f) || (raw&0x30)!=(channel==1?0x10:0x20))) {
        error(err,"O encoder do canal não está associado exclusivamente a este motor.");return {};
      }
      if(raw)raw=(raw&0x0f)|0x10;
    }
    entries.append(QJsonObject{{"query",b.canonicalQuery},{"raw",double(raw)},{"scale",spec->scale}});
  }
  QJsonObject profile{{"application","MotorLab Linux"},{"scope","motor_channel_profile"},{"schema",1},
    {"name",name.trimmed()},{"model",Protocol::supportedModel(model)},{"firmware",firmware},{"source",source},
    {"captured_channel",channel},{"timestamp_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    {"parameters",entries}};
  if(plan(profile,1,true,model,firmware,false,err).isEmpty())return {};
  return profile;
}
QJsonArray MotorProfiles::plan(const QJsonObject &p,int channel,bool calibration,const QString &model,
                              const QString &firmware,bool real,QString *err) {
  if(err)err->clear();
  auto bad=[&](const QString &message){error(err,message);return QJsonArray();};
  if(p["application"].toString()!="MotorLab Linux"||p["scope"].toString()!="motor_channel_profile"||
      p["schema"].toInt()!=1||p["name"].toString().trimmed().isEmpty()||p["name"].toString().size()>160||channel<1||channel>2)
    return bad("Formato de perfil de motor inválido.");
  const auto expected=Protocol::supportedModel(model);
  if(expected.isEmpty()||p["model"].toString()!=expected)return bad("O perfil pertence a outro modelo de controlador.");
  if(real && (p["source"].toString()=="demo"||p["firmware"].toString().isEmpty()||p["firmware"].toString()!=firmware))
    return bad("Perfil simulado ou firmware diferente da unidade conectada.");
  if(calibration && p.contains("calibration_available") && !p["calibration_available"].toBool())
    return bad("Este perfil não possui calibração validada para este modelo de motor.");
  if(!p["parameters"].isArray()||p["parameters"].toArray().size()!=bindings().size())return bad("Perfil de motor incompleto.");
  QMap<QString,Binding> allowed;for(const auto &b:bindings())allowed[b.canonicalQuery]=b;
  QSet<QString> seen;QJsonArray result;
  for(auto v:p["parameters"].toArray()) {
    const auto e=v.toObject();const auto query=e["query"].toString();
    if(!allowed.contains(query)||seen.contains(query))return bad("Parâmetro global, de outro canal ou duplicado no perfil: "+query);
    seen.insert(query);const auto b=allowed[query];const auto spec=Catalog::find(b.key);const double d=e["raw"].toDouble();
    if(!e["raw"].isDouble()||!std::isfinite(d)||std::floor(d)!=d||d<spec->minimum||d>spec->maximum||e["scale"].toDouble()!=spec->scale)
      return bad("Valor ou escala inválida em "+query);
    qint64 raw=qint64(d);
    if(b.key=="EMOD") {
      if(raw && ((raw&~0x3f) || (raw&0x30)!=0x10))return bad("Associação de encoder inválida no perfil.");
      if(raw)raw=(raw&0x0f)|(channel==1?0x10:0x20);
    }
    if(!Catalog::validValue(*spec,raw,channel==1?b.first:b.second))return bad("Valor incompatível com o canal: "+query);
    if(b.calibration&&!calibration)continue;
    result.append(QJsonObject{{"query",suffix(b,channel)},{"raw",double(raw)},{"calibration",b.calibration}});
  }
  return result;
}
QString MotorProfiles::directory() {return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/motor-profiles";}
QJsonArray MotorProfiles::library(const QString &given) {
  Catalog::all();QFile builtins(":/motor-profiles.json");QJsonArray result;
  if(builtins.open(QIODevice::ReadOnly))result=QJsonDocument::fromJson(builtins.readAll()).array();
  const QDir dir(storage(given));
  for(const auto &file:dir.entryList({"*.json"},QDir::Files,QDir::Name)) {
    QFile f(dir.filePath(file));if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)continue;
    auto p=QJsonDocument::fromJson(f.readAll()).object();QString err;
    if(plan(p,1,false,p["model"].toString(),p["firmware"].toString(),false,&err).isEmpty())continue;
    p["builtin"]=false;p["id"]=file.chopped(5);result.append(p);
  }
  return result;
}
bool MotorProfiles::save(QJsonObject p,QString *err,const QString &given) {
  if(plan(p,1,false,p["model"].toString(),p["firmware"].toString(),false,err).isEmpty())return false;
  const auto dir=storage(given);
  if(!QDir().mkpath(dir)) {error(err,"Não foi possível criar a biblioteca de perfis.");return false;}
  p.remove("builtin");p["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto bytes=QJsonDocument(p).toJson();QSaveFile f(dir+"/"+p["id"].toString()+".json");
  if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()) {error(err,"Não foi possível salvar o perfil.");return false;}
  return true;
}
bool MotorProfiles::remove(const QString &id,QString *err,const QString &given) {
  const QUuid uuid(id);
  if(uuid.isNull()||uuid.toString(QUuid::WithoutBraces)!=id||!QFile::remove(storage(given)+"/"+id+".json")) {
    error(err,"Não foi possível excluir o perfil pessoal.");return false;
  }
  return true;
}
