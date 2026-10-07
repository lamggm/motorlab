#include "basicvm.h"
#include "window.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QVBoxLayout>
void Window::showScriptSimulator() {
  BasicVm vm;
  if (!vm.compile(script->toPlainText())) {
    notify(vm.error);
    return;
  }
  const auto source = script->toPlainText();
  QDialog dialog(this);
  dialog.setWindowTitle("MicroBasic · simulação local");
  dialog.resize(900, 600);
  auto layout = new QVBoxLayout(&dialog);
  auto info =
      new QLabel("Simulação de inteiros, variáveis, arrays, If, While, For, "
                 "GoTo/GoSub, Print, Wait, timers e Get/Set.\nEntradas e "
                 "comandos existem apenas nesta simulação. Wait avança o "
                 "relógio virtual; não mede tempo de motor.");
  info->setWordWrap(true);
  layout->addWidget(info);
  auto tools = new QHBoxLayout;
  auto run = new QPushButton("Executar"), step = new QPushButton("Um passo"),
       pause = new QPushButton("Pausar"), reset = new QPushButton("Reiniciar"),
       input = new QPushButton("Definir entrada");
  for (auto b : {run, step, pause, reset, input})
    tools->addWidget(b);
  layout->addLayout(tools);
  auto status = new QLabel;
  layout->addWidget(status);
  auto split = new QHBoxLayout;
  auto variables = new QTableWidget(0, 2);
  variables->verticalHeader()->hide();
  variables->setHorizontalHeaderLabels(
      {"Variável / comando simulado", "Valor"});
  variables->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  variables->setEditTriggers(QAbstractItemView::NoEditTriggers);
  auto output = new QPlainTextEdit;
  output->setReadOnly(true);
  split->addWidget(variables);
  split->addWidget(output);
  layout->addLayout(split, 1);
  auto close = new QDialogButtonBox(QDialogButtonBox::Close);
  layout->addWidget(close);
  connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
  QTimer timer(&dialog);
  timer.setInterval(10);
  auto update = [&] {
    output->setPlainText(vm.output);
    auto state = vm.variables;
    for (auto it = vm.commands.begin(); it != vm.commands.end(); ++it)
      state["!" + it.key()] = it.value();
    variables->setRowCount(state.size());
    int row = 0;
    for (auto it = state.begin(); it != state.end(); ++it, ++row) {
      variables->setItem(row, 0, new QTableWidgetItem(it.key()));
      variables->setItem(row, 1,
                         new QTableWidgetItem(QString::number(it.value())));
    }
    status->setText(vm.error.isEmpty()
                        ? QString("Linha %1 · relógio virtual %2 ms · %3")
                              .arg(vm.line())
                              .arg(vm.milliseconds)
                              .arg(vm.done()          ? "encerrado"
                                   : timer.isActive() ? "executando"
                                                      : "pausado")
                        : vm.error);
    run->setEnabled(!vm.done());
    step->setEnabled(!vm.done() && !timer.isActive());
    pause->setEnabled(timer.isActive());
    input->setEnabled(!timer.isActive());
  };
  connect(&timer, &QTimer::timeout, &dialog, [&] {
    vm.run(100);
    if (vm.done())
      timer.stop();
    update();
  });
  connect(run, &QPushButton::clicked, &dialog, [&] {
    timer.start();
    update();
  });
  connect(pause, &QPushButton::clicked, &dialog, [&] {
    timer.stop();
    update();
  });
  connect(step, &QPushButton::clicked, &dialog, [&] {
    vm.step();
    update();
  });
  connect(reset, &QPushButton::clicked, &dialog, [&] {
    timer.stop();
    vm.compile(source);
    update();
  });
  connect(input, &QPushButton::clicked, &dialog, [&] {
    bool ok = false;
    auto text = QInputDialog::getText(&dialog, "Entrada simulada",
                                      "Consulta e valor bruto (ex.: ?A 1=125)",
                                      QLineEdit::Normal, "?A 1=125", &ok);
    if (!ok)
      return;
    auto parts = text.split('=');
    if (parts.size() != 2)
      return;
    auto command = Protocol::command(parts[0].trimmed());
    auto n = parts[1].trimmed().toLongLong(&ok);
    if (!command.valid || !command.text.startsWith('?') || !ok ||
        n < -2147483648LL || n > 2147483647LL) {
      status->setText("Consulta ou valor inválido.");
      return;
    }
    vm.setInput(command.key, command.channel, qint32(n));
    update();
  });
  update();
  dialog.exec();
}
