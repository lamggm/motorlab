#pragma once
#include "catalog.h"
#include <QMap>
#include <QObject>
#include <QSet>
#include <QSocketNotifier>
#include <QStringList>
#include <QTimer>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>
class FakeController : public QObject {
public:
  FakeController() {
    char name[128]{};
    int slave = -1;
    if (openpty(&master, &slave, name, nullptr, nullptr) != 0)
      qFatal("openpty failed");
    path = QString::fromLocal8Bit(name);
    ::close(slave);
    fcntl(master, F_SETFL, O_NONBLOCK);
    notifier = new QSocketNotifier(master, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] { consume(); });
    for (const auto &s : Catalog::all())
      for (int i = s.indexMax ? 1 : 0; i <= s.indexMax; ++i)
        params[Catalog::suffix(s, i)] = qMax<qint64>(0, s.minimum);
    params["RWD"] = 1000;
    params["ALIM 1"] = 200;
    params["ALIM 2"] = 200;
  }
  ~FakeController() override {
    if (master >= 0)
      ::close(master);
  }
  void send(const QString &value) {
    const auto b = (value + "\r\n").toLatin1();
    ::write(master, b.constData(), size_t(b.size()));
  }
  void consume() {
    char bytes[512];
    auto n = ::read(master, bytes, sizeof bytes);
    if (n <= 0)
      return;
    buffer.append(bytes, n);
    while (buffer.contains('\r')) {
      int pos = buffer.indexOf('\r');
      auto cmd = QString::fromLatin1(buffer.left(pos));
      buffer.remove(0, pos + 1);
      if (cmd.isEmpty())
        continue;
      commands << cmd;
      if (silent.contains(cmd))
        continue;
      QString answer;
      if (overrides.contains(cmd))
        answer = overrides[cmd];
      else if (cmd == "?FID")
        answer = "FID=Roboteq v3.0f SBLMG2XXX 07/15/2026";
      else if (cmd == "?TRN")
        answer = "TRN=RoboG4:SBLMG2360T";
      else if (cmd == "?A")
        answer = "A=125:43";
      else if (cmd == "?S")
        answer = "S=850:-210";
      else if (cmd == "?V 2")
        answer = "V=512";
      else if (cmd == "?T 2")
        answer = "T=37";
      else if (cmd == "?C")
        answer = "C=12000:-2400";
      else if (cmd.startsWith("?BS "))
        answer = "BS=0";
      else if (cmd.startsWith("?CB "))
        answer = "CB=0";
      else if (cmd.startsWith("?P "))
        answer = "P=0";
      else if (cmd.startsWith("?M "))
        answer = "M=0";
      else if (cmd == "?FF")
        answer = "FF=" + QString::number(faults);
      else if (cmd == "?FS")
        answer = "FS=1";
      else if (cmd == "?DI")
        answer = "DI=3";
      else if (cmd == "?DO")
        answer = "DO=1";
      else if (cmd.startsWith("?FM "))
        answer = "FM=0";
      else if (cmd.startsWith("?AI "))
        answer = "AI=2500";
      else if (cmd.startsWith("?PI "))
        answer = "PI=1500";
      else if (cmd == "%SLD")
        answer = "HLD";
      else if (cmd.startsWith(':'))
        answer = "+";
      else if (cmd.startsWith('!') || cmd.startsWith('%')) {
        if (cmd == "!EX")
          faults |= 16;
        if (cmd == "!MG")
          faults &= ~16;
        answer = "+";
      } else if (cmd.startsWith('~') && params.contains(cmd.mid(1))) {
        const auto suffix = cmd.mid(1);
        answer =
            suffix.section(' ', 0, 0) + "=" +
            QString::number(suffix.startsWith("ALIM ") ? alim : params[suffix]);
      } else if (cmd.startsWith('^')) {
        auto suffix = cmd.mid(1).section(' ', 0, cmd.count(' ') - 1);
        params[suffix] = cmd.section(' ', -1).toLongLong();
        if (suffix.startsWith("ALIM "))
          alim = params[suffix];
        answer = "+";
      } else
        answer = "?";
      if (cmd == "#" || cmd == "#C" || cmd.startsWith('/'))
        continue;
      auto response = (cmd + "\r\n" + answer + "\r\n").toLatin1();
      if (fragment) {
        int split = response.size() / 2;
        ::write(master, response.constData(), size_t(split));
        QTimer::singleShot(5, this, [this, response, split] {
          if (master >= 0)
            ::write(master, response.constData() + split,
                    size_t(response.size() - split));
        });
      } else
        ::write(master, response.constData(), size_t(response.size()));
    }
  }
  int master = -1, alim = 200, faults = 0;
  QString path;
  QStringList commands;
  QMap<QString, QString> overrides;
  QMap<QString, qint64> params;
  QSet<QString> silent;
  bool fragment = true;
  QByteArray buffer;
  QSocketNotifier *notifier = nullptr;
};
