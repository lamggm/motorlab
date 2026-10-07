#pragma once
#include <QByteArray>
#include <QLockFile>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <memory>
class SdoTransfer {
public:
  quint16 index = 0;
  quint8 sub = 0;
  bool active = false;
  QString error;
  QByteArray value;
  QByteArray begin(quint16 object, quint8 subindex);
  QByteArray accept(const QByteArray &data);

private:
  bool segmented = false, toggle = false;
  int expected = -1;
};
class CanSdo : public QObject {
  Q_OBJECT
public:
  explicit CanSdo(QObject *parent = nullptr);
  ~CanSdo() override;
  bool open(const QString &interface, int node);
  void close();
  bool read(quint16 index, quint8 sub);
  bool connected() const { return fd >= 0; }
  bool busy() const { return transfer.active; }
  QString error;
signals:
  void received(quint16 index, quint8 sub, QByteArray data, QString error);

private:
  bool send(const QByteArray &data);
  void consume();
  int fd = -1, node = 0;
  QSocketNotifier *reader = nullptr;
  QTimer deadline;
  SdoTransfer transfer;
  std::unique_ptr<QLockFile> lock;
};
