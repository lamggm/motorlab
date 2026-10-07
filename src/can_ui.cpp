#include "clickedit.h"
#include "cansdo.h"
#include "window.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>
void Window::showCanInspector() {
  if (session.connected()) {
    notify("Desconecte USB/serial antes de abrir o acesso CAN.");
    return;
  }
  QDialog dialog(this);
  dialog.setWindowTitle("CANopen · SocketCAN nativo");
  dialog.resize(750, 500);
  CanSdo can(&dialog);
  auto layout = new QVBoxLayout(&dialog);
  auto info = new QLabel(
      "Leitura SDO de configuração e identidade. Use uma interface SocketCAN "
      "já configurada.\nO modelo não é inferido do nó CAN: escrita, movimento "
      "e firmware por CAN permanecem indisponíveis nesta versão.");
  info->setWordWrap(true);
  layout->addWidget(info);
  auto row = new QHBoxLayout;
  auto interface = new QLineEdit("can0");
  auto node = new ClickEdit<QSpinBox>;
  node->setRange(1, 127);
  auto connection = new QPushButton("Conectar CAN");
  row->addWidget(interface);
  row->addWidget(node);
  row->addWidget(connection);
  layout->addLayout(row);
  auto objects = new ClickEdit<QComboBox>;
  for (auto e :
       QList<QPair<QString, quint16>>{{"Identidade / VendorID", 0x1018},
                                      {"Nome do dispositivo", 0x1008},
                                      {"Versão de software", 0x100a},
                                      {"Firmware FIN", 0x2137},
                                      {"Corrente motor", 0x2100},
                                      {"Tensão", 0x210d}})
    objects->addItem(e.first, e.second);
  for (const auto &s : Catalog::all())
    if (s.canIndex)
      objects->addItem(s.key + " · " + s.name, s.canIndex);
  row = new QHBoxLayout;
  row->addWidget(objects, 1);
  auto sub = new ClickEdit<QSpinBox>;
  sub->setRange(0, 255);
  sub->setValue(1);
  row->addWidget(sub);
  auto read = new QPushButton("Ler objeto");
  read->setEnabled(false);
  row->addWidget(read);
  layout->addLayout(row);
  auto log = new QPlainTextEdit;
  log->setReadOnly(true);
  log->setMaximumBlockCount(2000);
  layout->addWidget(log, 1);
  auto close = new QDialogButtonBox(QDialogButtonBox::Close);
  layout->addWidget(close);
  connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
  connect(connection, &QPushButton::clicked, &dialog, [&] {
    if (can.connected())
      can.close();
    else if (!can.open(interface->text(), node->value()))
      log->appendPlainText(can.error);
    connection->setText(can.connected() ? "Desconectar CAN" : "Conectar CAN");
    read->setEnabled(can.connected());
    interface->setEnabled(!can.connected());
    node->setEnabled(!can.connected());
  });
  connect(read, &QPushButton::clicked, &dialog, [&] {
    if (can.read(objects->currentData().toUInt(), sub->value()))
      read->setEnabled(false);
    else
      log->appendPlainText("Nó ocupado ou desconectado.");
  });
  connect(
      &can, &CanSdo::received, &dialog,
      [&](quint16 index, quint8 subindex, const QByteArray &data,
          const QString &error) {
        const auto value =
            QString("0x%1:%2 = ").arg(index, 4, 16, QChar('0')).arg(subindex) +
            (error.isEmpty() ? QString::fromLatin1(data.toHex(' ')) + " · " +
                                   QString::fromLatin1(data)
                             : error);
        log->appendPlainText(value);
        read->setEnabled(can.connected());
        connection->setText(can.connected() ? "Desconectar CAN"
                                            : "Conectar CAN");
        interface->setEnabled(!can.connected());
        node->setEnabled(!can.connected());
      });
  dialog.exec();
}
