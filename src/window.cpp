#include "clickedit.h"
#include "window.h"
#include "basicvm.h"
#include <memory>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextStream>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
QLabel *label(const QString &text, const QString &name = QString()) {
  auto l = new QLabel(text);
  l->setObjectName(name);
  l->setWordWrap(true);
  return l;
}
QPushButton *button(const QString &text, const QString &name = QString()) {
  auto b = new QPushButton(text);
  b->setObjectName(name);
  b->setCursor(Qt::PointingHandCursor);
  return b;
}
QPushButton *unavailable(const QString &text, const QString &name = QString()) {
  auto b = button(text, name);
  b->setEnabled(false);
  b->setToolTip("Ainda não implementado nesta versão.");
  return b;
}
struct Section {
  QFrame *frame;
  QVBoxLayout *body;
  QHBoxLayout *tools;
};
Section section(const QString &text) {
  auto frame = new QFrame;
  frame->setObjectName("panel");
  auto outer = new QVBoxLayout(frame);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);
  auto heading = new QFrame;
  heading->setObjectName("panelHeader");
  auto tools = new QHBoxLayout(heading);
  tools->setContentsMargins(9, 3, 6, 3);
  tools->setSpacing(5);
  auto title = label(text, "panelTitle");
  title->setWordWrap(false);
  tools->addWidget(title);
  tools->addStretch();
  auto bodyWidget = new QWidget;
  auto body = new QVBoxLayout(bodyWidget);
  body->setContentsMargins(8, 8, 8, 8);
  body->setSpacing(6);
  outer->addWidget(heading);
  outer->addWidget(bodyWidget, 1);
  return {frame, body, tools};
}
QString number(double n, int decimals = 1) {
  return std::isfinite(n) ? QString::number(n, 'f', decimals)
                          : QStringLiteral("—");
}
QVector<qint64> integers(const QString &payload) {
  const auto r = Protocol::reply("DATA=" + payload);
  return r.numeric ? r.numbers : QVector<qint64>{};
}
class BasicHighlighter : public QSyntaxHighlighter {
public:
  explicit BasicHighlighter(QTextDocument *doc) : QSyntaxHighlighter(doc) {}
  void highlightBlock(const QString &text) override {
    const QRegularExpression keywords(
        "\\b(Dim|As|Integer|Boolean|While|End|If|Then|Else|For|Next|Print|Wait|"
        "GetValue|SetCommand|SetConfig|Option|Explicit)\\b",
        QRegularExpression::CaseInsensitiveOption);
    auto matches = keywords.globalMatch(text);
    QTextCharFormat f;
    f.setForeground(QColor("#77adff"));
    f.setFontWeight(QFont::Bold);
    while (matches.hasNext()) {
      const auto m = matches.next();
      setFormat(m.capturedStart(), m.capturedLength(), f);
    }
    auto strings = QRegularExpression("\"[^\"]*\"").globalMatch(text);
    f.setForeground(QColor("#45d4b5"));
    f.setFontWeight(QFont::Normal);
    while (strings.hasNext()) {
      const auto m = strings.next();
      setFormat(m.capturedStart(), m.capturedLength(), f);
    }
    const auto comment = text.indexOf('\'');
    if (comment >= 0) {
      f.setForeground(QColor("#8195af"));
      setFormat(comment, text.size() - comment, f);
    }
  }
};
class ScriptEditor : public QPlainTextEdit {
public:
  ScriptEditor() {
    gutter = new QWidget(this);
    gutter->installEventFilter(this);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    connect(this, &QPlainTextEdit::blockCountChanged, this,
            [this] { updateMargin(); });
    connect(this, &QPlainTextEdit::updateRequest, this,
            [this](QRect rect, int dy) {
              if (dy)
                gutter->scroll(0, dy);
              else
                gutter->update(0, rect.y(), gutter->width(), rect.height());
              if (rect.contains(viewport()->rect()))
                updateMargin();
            });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] {
      QTextEdit::ExtraSelection selected;
      selected.format.setBackground(QColor("#16263c"));
      selected.format.setProperty(QTextFormat::FullWidthSelection, true);
      selected.cursor = textCursor();
      selected.cursor.clearSelection();
      setExtraSelections({selected});
    });
    updateMargin();
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QPlainTextEdit::resizeEvent(event);
    updateMargin();
  }
  bool eventFilter(QObject *object, QEvent *event) override {
    if (object != gutter || event->type() != QEvent::Paint)
      return QPlainTextEdit::eventFilter(object, event);
    QPainter p(gutter);
    p.fillRect(gutter->rect(), QColor("#101e31"));
    p.setPen(QColor("#6e86a6"));
    p.setFont(font());
    auto block = firstVisibleBlock();
    int top =
        qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid() && top <= gutter->height()) {
      const int height = qRound(blockBoundingRect(block).height());
      if (block.isVisible() && top + height >= 0)
        p.drawText(0, top, gutter->width() - 7, fontMetrics().height(),
                   Qt::AlignRight, QString::number(block.blockNumber() + 1));
      top += height;
      block = block.next();
    }
    return true;
  }

private:
  void updateMargin() {
    const int digits = QString::number(qMax(1, blockCount())).size();
    const int width = 15 + digits * fontMetrics().horizontalAdvance('9');
    setViewportMargins(width, 0, 0, 0);
    const auto area = contentsRect();
    gutter->setGeometry(area.left(), area.top(), width, area.height());
  }
  QWidget *gutter = nullptr;
};
} // namespace

Window::Window(QWidget *parent) : QMainWindow(parent), session(this) {
  buildUi();
  connect(&session, &Session::changed, this, &Window::refreshState);
  connect(&session, &Session::received, this, &Window::onReply);
  connect(&session, &Session::traffic, this, &Window::writeLog);
  connect(&session, &Session::failed, this, [this](const QString &message) {
    if (applying)
      completeApply(
          false,
          "Operação interrompida; alguns valores podem ter sido alterados. " +
              message);
    notify(message);
    writeLog("ERRO", message);
  });
  connect(&session, &Session::streamFrame, this, &Window::onStream);
  connect(&session, &Session::uploadProgress, this,
          [this](int done, int total) {
            notify(QString("Transferência HEX: %1 / %2 registros")
                       .arg(done)
                       .arg(total));
          });
  connect(&session, &Session::uploadFinished, this,
          [this](bool ok, const QString &message) {
            applying = false;
            refreshState();
            notify(message);
            writeLog(ok ? "OK" : "ERRO", message);
          });
  pollTimer.setInterval(500);
  connect(&pollTimer, &QTimer::timeout, this, &Window::poll);
  pollTimer.start();
  refreshPorts();
  refreshState();
}

void Window::buildUi() {
  setWindowTitle("MotorLab Linux 0.6.0 • SBLG2360T / SBLMG2360T");
  resize(1440, 860);
  setMinimumSize(1100, 680);
  setStyleSheet(R"(
        QWidget { color: #dbe6f7; font-family: 'DejaVu Sans'; font-size: 11px; }
        QMainWindow, QWidget#root, QScrollArea, QWidget#scrollContent { background: #0b1322; }
        QDialog { background: #111d30; }
        QFrame#header { background: #0e1a2d; border-bottom: 1px solid #263950; }
        QFrame#footer { background: #0e1a2d; border-top: 1px solid #263950; }
        QFrame#panel { background: #111d30; border: 1px solid #2d435e; border-radius: 3px; }
        QFrame#panelHeader { background: #203b5d; border: 0; border-radius: 2px; }
        QLabel { background: transparent; }
        QLabel#brand { font-size: 30px; font-weight: 700; color: #f1f6ff; }
        QLabel#muted { color: #8fa1ba; }
        QLabel#panelTitle { font-weight: 600; color: #b9d9ff; }
        QLabel#status { color: #64e0c8; font-weight: 600; }
        QLabel#detectedModel { background: #0d1829; border: 1px solid #33455f; padding: 5px 8px; color: #b9d9ff; }
        QLabel#notice { color: #b9cde8; }
        QTabBar { background: #111d30; }
        QTabBar::tab { background: #142239; color: #94abc8; border: 1px solid #273d56; padding: 8px 27px; min-width: 85px; }
        QTabBar::tab:selected { background: #203b5d; color: #d9ebff; border-bottom: 2px solid #63a8ff; }
        QTabBar::tab:hover { background: #1a304c; }
        QPushButton { background: #1c2c43; border: 1px solid #344661; border-radius: 3px; padding: 5px 9px; }
        QPushButton:hover { background: #263b58; border-color: #608cc2; }
        QPushButton:pressed { background: #193452; }
        QPushButton:disabled { color: #60738c; background: #152135; border-color: #293b53; }
        QPushButton#connectButton, QPushButton#applyButton { background: #3576d8; color: white; border-color: #4b8ff0; font-weight: 600; }
        QPushButton#applyButton:disabled { background: #1c304d; color: #607da3; border-color: #2c4260; }
        QComboBox, QLineEdit, QDoubleSpinBox, QSpinBox { background: #0d1829; border: 1px solid #33455f; border-radius: 2px; padding: 4px 7px; }
        QComboBox:disabled, QDoubleSpinBox:disabled, QLineEdit:disabled, QSpinBox:disabled { color: #60738c; }
        QComboBox::drop-down { border: 0; width: 20px; }
        QComboBox QAbstractItemView { background: #15253d; selection-background-color: #28578a; }
        QPlainTextEdit { background: #0b1728; border: 1px solid #2c3c55; border-radius: 2px; padding: 3px; font-family: 'DejaVu Sans Mono'; font-size: 12px; selection-background-color: #28578a; }
        QTreeWidget, QTableWidget, QListWidget { background: #111d30; alternate-background-color: #142239; border: 0; outline: 0; gridline-color: #273952; }
        QTreeWidget::item { min-height: 26px; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: #203b5d; color: #d9ebff; }
        QTreeWidget::item:disabled { color: #667a94; }
        QListWidget::item { padding: 7px 5px; }
        QHeaderView::section { background: #182d47; color: #95abc9; border: 0; padding: 6px; font-weight: 600; }
        QCheckBox { spacing: 5px; }
        QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #567298; background: #13253c; border-radius: 2px; }
        QCheckBox::indicator:checked { background: #438ceb; border-color: #69a5f3; }
        QCheckBox::indicator:disabled { background: #293c55; border-color: #40556f; }
        QSlider::groove:horizontal { height: 4px; background: #293c55; }
        QSlider::handle:horizontal { width: 10px; margin: -5px 0; background: #608cc2; }
        QScrollBar:vertical { background: #111e31; width: 10px; }
        QScrollBar::handle:vertical { background: #38506d; min-height: 20px; border-radius: 3px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: #111e31; height: 10px; }
        QScrollBar::handle:horizontal { background: #38506d; min-width: 20px; border-radius: 3px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QSplitter::handle { background: #0b1322; width: 6px; height: 6px; }
        QMessageBox { background: #111d30; }
    )");
  auto root = new QWidget;
  root->setObjectName("root");
  setCentralWidget(root);
  auto layout = new QVBoxLayout(root);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  auto header = new QFrame;
  header->setObjectName("header");
  auto head = new QHBoxLayout(header);
  head->setContentsMargins(16, 10, 12, 10);
  head->setSpacing(24);
  auto branding = new QVBoxLayout;
  branding->setSpacing(1);
  auto brand = label("MOTORLAB", "brand");
  brand->setWordWrap(false);
  branding->addWidget(brand);
  auto caption = label("MOTOR CONTROL UTILITY  ·  Linux 0.6.0", "muted");
  caption->setWordWrap(false);
  branding->addWidget(caption);
  auto supported = label("SBLG2360T / SBLMG2360T  ·  dois canais", "muted");
  supported->setWordWrap(false);
  branding->addWidget(supported);
  head->addLayout(branding);
  head->addStretch();
  auto scriptControls = new QVBoxLayout;
  scriptControls->setSpacing(4);
  scriptControls->addWidget(label("MicroBasic no controlador", "muted"));
  auto scriptButtons = new QHBoxLayout;
  scriptButtons->setSpacing(4);
  for (const auto &entry : QList<QPair<QString, int>>{
           {"Executar", 1}, {"Pausar", 0}, {"Reiniciar", 2}}) {
    const QString name = entry.second == 1   ? "runScript"
                         : entry.second == 0 ? "pauseScript"
                                             : "restartScript";
    auto b = button(entry.first, name);
    scriptButtons->addWidget(b);
    liveActions.append(b);
    connect(b, &QPushButton::clicked, this, [this, entry] {
      runtime(QString("!R %1").arg(entry.second),
              "Controlar o script carregado no controlador");
    });
  }
  scriptControls->addLayout(scriptButtons);
  scriptControls->addWidget(label("Script deve estar carregado", "muted"));
  head->addLayout(scriptControls);
  head->addStretch();
  auto controls = new QGridLayout;
  controls->setHorizontalSpacing(6);
  controls->setVerticalSpacing(5);
  controls->addWidget(label("Fonte", "muted"), 0, 0);
  source = new ClickEdit<QComboBox>;
  source->setObjectName("sourceSelector");
  source->addItems({"Demonstração", "USB / Serial"});
  controls->addWidget(source, 0, 1);
  detectedModel = label("Detectado: —", "detectedModel");
  detectedModel->setMinimumWidth(165);
  detectedModel->setWordWrap(false);
  controls->addWidget(detectedModel, 0, 2, 1, 2);
  controls->addWidget(label("Porta", "muted"), 1, 0);
  port = new ClickEdit<QComboBox>;
  port->setObjectName("portSelector");
  port->setEditable(true);
  port->setMinimumWidth(170);
  port->lineEdit()->setPlaceholderText("/dev/ttyACM0");
  controls->addWidget(port, 1, 1, 1, 2);
  portRefresh = button("↻", "refreshPorts");
  portRefresh->setToolTip("Atualizar lista de portas");
  controls->addWidget(portRefresh, 1, 3);
  controls->addWidget(label("Baud", "muted"), 2, 0);
  baud = new ClickEdit<QComboBox>;
  baud->addItems({"115200", "57600", "38400", "19200", "9600"});
  controls->addWidget(baud, 2, 1);
  connection = button("Abrir demo", "connectButton");
  controls->addWidget(connection, 2, 2, 1, 2);
  head->addLayout(controls);
  layout->addWidget(header);
  connect(connection, &QPushButton::clicked, this, &Window::connectSource);
  connect(portRefresh, &QPushButton::clicked, this, &Window::refreshPorts);
  connect(source, &QComboBox::currentIndexChanged, this, &Window::refreshState);
  navigation = new QTabBar;
  navigation->setObjectName("navigation");
  navigation->setExpanding(false);
  for (const auto &tab : {"Configuração", "Monitoramento", "Console",
                          "Diagnóstico", "Scripts", "Sobre"})
    navigation->addTab(tab);
  layout->addWidget(navigation);
  auto workspace = new QWidget;
  auto body = new QVBoxLayout(workspace);
  body->setContentsMargins(8, 8, 8, 8);
  body->setSpacing(0);
  pages = new QStackedWidget;
  pages->setObjectName("workspacePages");
  pages->addWidget(parametersPage());
  pages->addWidget(monitorPage());
  pages->addWidget(consolePage());
  pages->addWidget(diagnosticsPage());
  pages->addWidget(scriptsPage());
  pages->addWidget(aboutPage());
  body->addWidget(pages);
  layout->addWidget(workspace, 1);
  auto footer = new QFrame;
  footer->setObjectName("footer");
  auto foot = new QHBoxLayout(footer);
  foot->setContentsMargins(10, 5, 10, 5);
  foot->setSpacing(15);
  notice = label("Selecione uma fonte para começar.", "notice");
  notice->setWordWrap(false);
  notice->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  foot->addWidget(notice, 1);
  footerFirmware = label("Firmware: —", "muted");
  footerFirmware->setWordWrap(false);
  footerFirmware->setMaximumWidth(430);
  footerFirmware->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  foot->addWidget(footerFirmware, 1);
  status = label("DESCONECTADO", "status");
  status->setWordWrap(false);
  foot->addWidget(status);
  layout->addWidget(footer);
  connect(navigation, &QTabBar::currentChanged, this, &Window::selectPage);
  selectPage(0);
}

QWidget *Window::parametersPage() {
  auto page = new QWidget;
  auto outer = new QVBoxLayout(page);
  outer->setContentsMargins(0, 0, 0, 0);
  auto filter = new QLineEdit;
  filter->setObjectName("parameterFilter");
  filter->setPlaceholderText(
      "Filtrar por nome ou comando (ex.: CAN, ALIM, Hall)");
  outer->addWidget(filter);
  auto split = new QSplitter(Qt::Horizontal);
  split->setObjectName("configurationSplitter");
  split->setChildrenCollapsible(false);
  split->setHandleWidth(7);
  auto sections = new QTabWidget;
  sections->setObjectName("configurationSections");
  sections->setMinimumWidth(700);
  sections->setDocumentMode(true);
  auto inputs = section("Entradas / saídas");
  auto power = section("Saída de potência");
  ioTree = new QTreeWidget;
  ioTree->setObjectName("ioCategories");
  parameterTree = new QTreeWidget;
  parameterTree->setObjectName("parametersTree");
  for (auto tree : {ioTree, parameterTree}) {
    tree->setColumnCount(4);
    tree->setHeaderLabels({"Parâmetro", "Atual", "Novo valor", "Unidade"});
    tree->setIndentation(13);
    tree->setAlternatingRowColors(true);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setStyleSheet(R"(
      QTreeWidget { font-size: 13px; }
      QTreeWidget::item { min-height: 36px; padding: 2px 0; }
      QHeaderView::section { font-size: 12px; font-weight: 600; padding: 10px 8px; }
      QDoubleSpinBox, QComboBox { font-size: 14px; min-height: 26px;
        padding: 4px 28px 4px 10px; background: #142a43; border: 1px solid #52769c; }
      QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #63a8ff; }
      QDoubleSpinBox:disabled, QComboBox:disabled { color: #9aabc0;
        background: #101e30; border-color: #354b65; }
      QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 22px; }
      QComboBox::drop-down { width: 26px; }
    )");
    tree->header()->setStretchLastSection(false);
    tree->header()->setMinimumSectionSize(90);
    tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->setColumnWidth(1, 112);
    tree->setColumnWidth(2, 250);
    tree->setColumnWidth(3, 145);
    tree->header()->setToolTip("Arraste as divisórias das colunas para ajustar a largura.");
  }
  auto general = new QTreeWidgetItem(parameterTree, {"Geral"});
  general->setFirstColumnSpanned(true);
  QVector<QTreeWidgetItem *> motors;
  for (int i = 1; i <= 2; ++i) {
    auto m = new QTreeWidgetItem(parameterTree, {QString("Motor %1").arg(i)});
    m->setExpanded(true);
    m->setFirstColumnSpanned(true);
    motors.append(m);
  }
  QMap<QString, QTreeWidgetItem *> groups;
  QMap<QString, QTreeWidgetItem *> originalGroups;
  for (const auto &name : QStringList{
           "Start-up", "Commands", "RS232/485 Settings", "CAN", "ModBus",
           "Encoders", "SSI Sensors", "Digital Inputs", "Analog Inputs",
           "Pulse Inputs", "Digital Outputs", "User-Defined Values"}) {
    auto node = new QTreeWidgetItem(ioTree, {name});
    node->setFirstColumnSpanned(true);
    originalGroups["ControlTree|" + name] = node;
  }
  for (int i = 0; i < 2; ++i)
    for (const auto &name :
         QStringList{"Motor Configuration", "Motor Output"}) {
      const auto root = QString("PowerTree|Motor %1").arg(i + 1);
      originalGroups[root] = motors[i];
      auto node = new QTreeWidgetItem(motors[i], {name});
      node->setFirstColumnSpanned(true);
      originalGroups[root + "|" + name] = node;
    }

  // Put familiar motor limits first; retain stable command identities in all
  // operations.
  QVector<const ConfigSpec *> specs;
  for (const auto &key : {"ALIM", "MXRPM"})
    if (auto spec = Catalog::find(key))
      specs.append(spec);
  for (const auto &spec : Catalog::all())
    if (spec.key != "ALIM" && spec.key != "MXRPM")
      specs.append(&spec);
  for (const auto spec : specs)
    for (int index = spec->indexMax ? 1 : 0; index <= spec->indexMax; ++index) {
      Parameter p;
      p.key = spec->key;
      p.name = spec->name;
      p.unit = spec->indexUnits.value(QString::number(index)).toString(spec->unit);
      p.channel = index;
      p.scale = spec->scale;
      p.editable = spec->editable;
      p.spec = spec;
      p.suffix = Catalog::suffix(*spec, index);
      QTreeWidgetItem *parent = nullptr;
      if (spec->group == "Geral")
        parent = general;
      else if (spec->group == "Motor / potência") {
        // PID speed/position/FOC and Hall table use flat indices, not new motor
        // channels.
        const int motor = spec->key == "HSAT"   ? (index - 1) / 12
                          : spec->key == "ZSMC" ? (index - 1) / 3
                                                : (qMax(1, index) - 1) % 2;
        parent = motors[qBound(0, motor, 1)];
      } else {
        QString group = spec->group;
        const QMap<QString, QString> aliases{
            {"Comandos / segurança", "Commands"},
            {"Entradas analógicas", "Analog Inputs"},
            {"Entradas de pulso", "Pulse Inputs"},
            {"Sensores SSI", "SSI Sensors"}};
        if (aliases.contains(group))
          group = aliases[group];
        if (group == "Entradas / saídas digitais")
          group = p.key.startsWith("DO") ? "Digital Outputs" : "Digital Inputs";
        if (originalGroups.contains("ControlTree|" + group))
          parent = originalGroups["ControlTree|" + group];
        else {
          if (!groups.contains(group))
            groups[group] = new QTreeWidgetItem(ioTree, {group});
          parent = groups[group];
        }
        parent->setFirstColumnSpanned(true);
      }
      const auto meta = Catalog::commonOriginal(p.key, index);
      const auto originalPath = meta["path"].toArray();
      if (!originalPath.isEmpty()) {
        auto tree =
            meta["tree"].toString() == "PowerTree" ? parameterTree : ioTree;
        QTreeWidgetItem *branch = nullptr;
        QString prefix = meta["tree"].toString();
        for (int n = 0; n < originalPath.size() - 1; ++n) {
          const auto name = originalPath[n].toString();
          prefix += "|" + name;
          if (!originalGroups.contains(prefix)) {
            QTreeWidgetItem *node = nullptr;
            if (!branch && tree == parameterTree && name == "General")
              node = general;
            else if (!branch && tree == parameterTree && name == "Motor 1")
              node = motors[0];
            else if (!branch && tree == parameterTree && name == "Motor 2")
              node = motors[1];
            else
              node = branch ? new QTreeWidgetItem(branch, {name})
                            : new QTreeWidgetItem(tree, {name});
            node->setFirstColumnSpanned(true);
            originalGroups[prefix] = node;
          }
          branch = originalGroups[prefix];
        }
        if (branch)
          parent = branch;
      }
      const QString indexText = index ? QString(" [%1]").arg(index) : QString();
      auto item = new QTreeWidgetItem(
          parent, {p.name + indexText + " · " + p.key, "—", "Leitura", p.unit});
      item->setToolTip(0, QString("%1; manual v3.0c, página %2. Índice e valor "
                                  "na escala documentada.")
                              .arg(p.suffix)
                              .arg(spec->page));
      if (!meta.isEmpty())
        item->setToolTip(0, item->toolTip(0) +
                                "\nRoboRun+ 3.3 · árvore v3.0: " +
                                meta["description"].toString());
      item->setData(0, Qt::UserRole, p.suffix);
      item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
      item->setToolTip(3, p.unit + (spec->unitNote.isEmpty()
                                      ? QString()
                                      : "\n" + spec->unitNote));
      if (!spec->unitNote.isEmpty())
        item->setToolTip(0, item->toolTip(0) + "\n" + spec->unitNote);
      const int row = parameters.size();
      parameterItems.append(item);
      if (p.editable) {
        p.editor = new ClickEdit<QDoubleSpinBox>;
        p.editor->setObjectName(QString("draft_%1_%2").arg(p.key).arg(index));
        const int decimals = p.scale == 1000000 ? 6
                             : p.scale == 10000 ? 4
                             : p.scale == 1000  ? 3
                             : p.scale == 10    ? 1
                                                : 0;
        p.editor->setDecimals(decimals);
        p.editor->setRange(spec->minimum / p.scale, spec->maximum / p.scale);
        p.editor->setSingleStep(1 / p.scale);
        p.editor->setEnabled(false);
        const auto options = meta["control"].toString() == "ComboBox"
                                 ? meta["choices"].toArray()
                                 : QJsonArray();
        if (!options.isEmpty()) {
          p.choices = new ClickEdit<QComboBox>;
          p.choices->setObjectName(
              QString("choice_%1_%2").arg(p.key).arg(index));
          p.choices->view()->setMinimumWidth(430);
          p.editor->setParent(p.choices);
          p.editor->hide();
          for (auto option : options) {
            auto o = option.toObject();
            if (!o["label"].toString().trimmed().isEmpty() &&
                !o["label"].toString().trimmed().startsWith('~'))
              p.choices->addItem(o["label"].toString(), o["value"].toInt());
          }
          p.choices->setCurrentIndex(
              p.choices->findData(qRound(p.editor->value() * p.scale)));
          item->treeWidget()->setItemWidget(item, 2, p.choices);
          connect(p.choices, &QComboBox::currentIndexChanged, this,
                  [this, row](int at) {
                    if (at >= 0)
                      parameters[row].editor->setValue(
                          parameters[row].choices->itemData(at).toDouble() /
                          parameters[row].scale);
                    parameters[row].choices->setToolTip(
                        parameters[row].choices->currentText());
                  });
        } else
          item->treeWidget()->setItemWidget(item, 2, p.editor);
        connect(p.editor, &QDoubleSpinBox::valueChanged, this, [this, row] {
          parameters[row].dirty = true;
          parameterItems[row]->setText(0, parameters[row].name + " · " +
                                              parameters[row].suffix + " *");
          if (parameters[row].key == "PMOD" || parameters[row].key == "MMOD")
            for (int i = 0; i < parameters.size(); ++i)
              if (parameters[i].channel == parameters[row].channel)
                updateParameterRow(i);
        });
      }
      parameters.append(p);
    }
  for (auto group : groups)
    if (group->childCount() == 0)
      group->setHidden(true);
  inputs.body->addWidget(ioTree, 1);
  power.body->addWidget(parameterTree, 1);
  inputs.body->addWidget(
      label("Disponibilidade confirmada pelas respostas da unidade.", "muted"));
  power.body->addWidget(
      label("* Alteração pendente; ganhos PID/FOC usam a escala do manual.",
            "muted"));
  inputs.frame->findChild<QFrame *>("panelHeader")->hide();
  power.frame->findChild<QFrame *>("panelHeader")->hide();
  sections->addTab(inputs.frame, "Entradas / saídas");
  sections->addTab(power.frame, "Saída de potência");
  sections->setCurrentIndex(1);
  split->addWidget(sections);
  auto actions = section("Ações");
  actions.frame->setMinimumWidth(225);
  auto advanced = new QWidget;
  advanced->setObjectName("advancedConfigurationActions");
  auto advancedLayout = new QVBoxLayout(advanced);
  advancedLayout->setContentsMargins(0, 6, 0, 0);
  advancedLayout->setSpacing(6);
  offlineModel = new ClickEdit<QComboBox>;
  offlineModel->setObjectName("offlineModel");
  offlineModel->addItems({"SBLG2360T", "SBLMG2360T"});
  advancedLayout->addWidget(label("Modelo para perfil offline", "muted"));
  advancedLayout->addWidget(offlineModel);
  connect(offlineModel, &QComboBox::currentTextChanged, this,
          [this](const QString &model) {
            if (session.connected() || profileModel.isEmpty() ||
                model == profileModel)
              return;
            for (auto &p : parameters) {
              p.known = false;
              p.dirty = false;
              if (p.editor) {
                QSignalBlocker block(p.editor);
                p.editor->setValue(p.editor->minimum());
              }
            }
            profileModel = model;
            profileFirmware.clear();
            profileSource = "offline";
            refreshState();
            notify("Modelo offline alterado; os valores do perfil anterior "
                   "foram descartados.");
          });
  readParams = button("Ler do controlador", "readParameters");
  actions.body->addWidget(readParams);
  readProgress = label("Valores atuais ainda não lidos.", "readProgress");
  actions.body->addWidget(readProgress);
  editEnabled = new QCheckBox("Habilitar edição");
  editEnabled->setObjectName("enableWrites");
  editEnabled->setToolTip("Permite editar rascunhos. Use Aplicar alterações para enviá-los ao controlador.");
  actions.body->addWidget(editEnabled);
  applyParams = button("Aplicar alterações (RAM)", "applyButton");
  applyParams->setToolTip("Revisa e aplica os rascunhos à configuração temporária. Para conservar após desligar, salve na EEPROM depois.");
  actions.body->addWidget(applyParams);
  auto permanent = button("Salvar permanente (EEPROM)", "EESAV");
  permanent->setToolTip("Grava a configuração que já está na RAM do controlador. Rascunhos ainda não aplicados não são incluídos.");
  actions.body->addWidget(permanent);
  actions.body->addWidget(label("RAM: para testar. EEPROM: mantém após desligar.", "muted"));
  liveActions.append(permanent);
  connect(permanent, &QPushButton::clicked, this, [this] {
    maintenance("%EESAV", "Salvar permanente (EEPROM)",
                "Gravar a configuração atual do controlador para os próximos acionamentos. "
                "Rascunhos da tela não aplicados ficam fora desta gravação.");
  });
  actions.body->addSpacing(8);
  actions.body->addWidget(label("Arquivos no computador", "muted"));
  snapshot = button("Salvar perfil…", "saveSnapshot");
  snapshot->setToolTip("Exporta um arquivo JSON no computador; não grava a memória do driver.");
  actions.body->addWidget(snapshot);
  auto load = button("Abrir perfil…", "loadProfile");
  load->setToolTip("Abre um JSON como rascunho para revisão; não aplica automaticamente ao driver.");
  actions.body->addWidget(load);
  connect(load, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getOpenFileName(
        this, "Carregar perfil", QString(), "Perfil (*.json)");
    QString error;
    if (!path.isEmpty())
      notify(loadProfile(path, &error)
                 ? "Perfil carregado como rascunho. Revise antes de aplicar."
                 : error);
  });
  auto advancedToggle = new QCheckBox("Modo avançado");
  advancedToggle->setObjectName("advancedConfigurationToggle");
  advancedToggle->setToolTip("Exibe importação RoboRun, calibrações, recuperação e proteção da configuração. Não altera permissões nem parâmetros.");
  actions.body->addSpacing(8);
  actions.body->addWidget(advancedToggle);
  actions.body->addWidget(advanced);
  advanced->hide();
  connect(advancedToggle, &QCheckBox::toggled, advanced, &QWidget::setVisible);
  auto cpr = button("Abrir RoboRun .cpr", "importCpr");
  advancedLayout->addWidget(cpr);
  connect(cpr, &QPushButton::clicked, this, [this] { showCpr(); });
  auto compareCpr = button("Comparar .cpr", "compareCpr");
  advancedLayout->addWidget(compareCpr);
  connect(compareCpr, &QPushButton::clicked, this, [this] { showCpr(true); });
  auto exportCpr = button("Exportar .cpr parcial", "exportCpr");
  advancedLayout->addWidget(exportCpr);
  connect(exportCpr, &QPushButton::clicked, this, [this] {
    auto path = QFileDialog::getSaveFileName(this, "Exportar perfil parcial",
                                             "perfil.cpr", "RoboRun (*.cpr)");
    if (!path.isEmpty())
      notify(saveCpr(path)
                 ? "CPR parcial exportado: " + path
                 : "Nenhum valor exportável ou operação em andamento.");
  });
  for (const auto &entry : QList<QPair<QString, QString>>{
           {"Recarregar da EEPROM", "%EELD"},
           {"Restaurar padrões", "%EERST 321654987"},
           {"Salvar calibrações", "%CLSAV 321654987"}}) {
    auto b = button(entry.first);
    advancedLayout->addWidget(b);
    liveActions.append(b);
    b->setObjectName(entry.second.section(' ', 0, 0).mid(1));
    const QString description = entry.second.startsWith("%EERST")
        ? "Apagar a configuração da RAM e EEPROM e restaurar os padrões de fábrica."
        : entry.second.startsWith("%EELD")
        ? "Substituir a configuração temporária pela última configuração salva na EEPROM."
        : "Gravar os dados de calibração atuais na Flash do driver. Esta ação não executa uma calibração e não substitui Salvar permanente (EEPROM).";
    b->setToolTip(description);
    connect(b, &QPushButton::clicked, this, [this, entry, description] {
      maintenance(entry.second, entry.first, description);
    });
  }
  auto sensors = button("Motor / sensores", "sensorSetup");
  advancedLayout->addWidget(sensors);
  connect(sensors, &QPushButton::clicked, this, &Window::showSensorSetup);
  auto lock = button("Bloquear / desbloquear", "configurationLock");
  advancedLayout->addWidget(lock);
  liveActions.append(lock);
  connect(lock, &QPushButton::clicked, this, [this] {
    bool ok = false;
    const QString action =
        QInputDialog::getItem(this, "Proteção da configuração", "Operação",
                              {"Bloquear", "Desbloquear"}, 0, false, &ok);
    if (!ok)
      return;
    const QString key =
        QInputDialog::getText(this, action, "Chave numérica de 1 a 4294967295",
                              QLineEdit::Password, QString(), &ok);
    if (!ok)
      return;
    const QString cmd = (action == "Bloquear" ? "%LK " : "%UK ") + key;
    if (!Protocol::command(cmd).valid) {
      notify("Chave inválida.");
      return;
    }
    maintenance(
        cmd, action,
        "A leitura de parâmetros ficará protegida pela chave escolhida.");
  });
  actions.body->addStretch();
  auto scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setMinimumWidth(245);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(actions.frame);
  split->addWidget(scroll);
  split->setStretchFactor(0, 1);
  split->setStretchFactor(1, 0);
  split->setSizes({1140, 250});
  outer->addWidget(split, 1);
  connect(readParams, &QPushButton::clicked, this, &Window::readParameters);
  connect(applyParams, &QPushButton::clicked, this, &Window::applyParameters);
  connect(editEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
    if (session.connected() && !session.setWritesEnabled(enabled)) {
      QSignalBlocker b(editEnabled);
      editEnabled->setChecked(false);
      notify("Edição requer modelo suportado e firmware v3.0f.");
    }
    refreshState();
  });
  connect(snapshot, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getSaveFileName(
        this, "Salvar perfil", "perfil-motorlab.json", "JSON (*.json)");
    if (!path.isEmpty())
      notify(saveProfile(path)
                 ? "Perfil exportado: " + path
                 : "Não há dados conhecidos ou rascunhos válidos para salvar.");
  });
  connect(filter, &QLineEdit::textChanged, this, [this, sections](const QString &text) {
    int matches[2] = {0, 0};
    for (int i = 0; i < parameters.size(); ++i) {
      auto item = parameterItems[i];
      const bool match = text.isEmpty() ||
          QString(parameters[i].name + parameters[i].suffix + parameters[i].spec->group)
              .contains(text, Qt::CaseInsensitive);
      item->setHidden(!match);
      if (match) {
        ++matches[item->treeWidget() == ioTree ? 0 : 1];
        if (!text.isEmpty())
          for (auto parent = item->parent(); parent; parent = parent->parent())
            parent->setExpanded(true);
      }
    }
    if (!text.isEmpty() && !matches[sections->currentIndex()] &&
        matches[1 - sections->currentIndex()])
      sections->setCurrentIndex(1 - sections->currentIndex());
  });
  return page;
}

QTableWidget *Window::signalTable(const QString &name) {
  auto feed = new QTableWidget(6, 4);
  feed->setObjectName(name);
  feed->setHorizontalHeaderLabels({"Sinal", "Valor", "Mín.", "Máx."});
  feed->verticalHeader()->hide();
  feed->setEditTriggers(QAbstractItemView::NoEditTriggers);
  feed->setSelectionMode(QAbstractItemView::NoSelection);
  feed->setAlternatingRowColors(true);
  feed->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < 4; ++column)
    feed->setColumnWidth(column, 60);
  const QStringList names{"● Corrente M1 · A", "● Corrente M2 · A",
                          "● Bateria · V",     "● RPM M1",
                          "● RPM M2",          "● Dissipador · °C"};
  const QStringList colors{"#63a8ff", "#45d4b5", "#d5b665",
                           "#63a8ff", "#45d4b5", "#d5b665"};
  for (int row = 0; row < 6; ++row) {
    feed->setItem(row, 0, new QTableWidgetItem(names[row]));
    feed->item(row, 0)->setForeground(QColor(colors[row]));
    for (int column = 1; column < 4; ++column) {
      feed->setItem(row, column, new QTableWidgetItem("—"));
      feed->item(row, column)
          ->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    feed->setRowHeight(row, 28);
  }
  return feed;
}

QWidget *Window::monitorPage() {
  auto page = new QWidget;
  auto layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto live = new QHBoxLayout;
  live->setSpacing(6);
  metrics.resize(6);
  auto state = section("Estado / encoders");
  statusFlags = label("—");
  encoders = label("—");
  state.body->addWidget(label("Flags FS", "muted"));
  state.body->addWidget(statusFlags);
  state.body->addWidget(label("Contadores", "muted"));
  state.body->addWidget(encoders);
  state.body->addStretch();
  live->addWidget(state.frame, 2);
  auto readout = [this](QVBoxLayout *body, const QString &caption, int index,
                        const QString &unit) {
    auto row = new QHBoxLayout;
    row->addWidget(label(caption, "muted"), 1);
    auto value = label("—", "metric" + QString::number(index));
    value->setStyleSheet("font-size:22px; font-weight:600; color:#edf5ff;");
    value->setAlignment(Qt::AlignRight);
    metrics[index] = value;
    row->addWidget(value);
    auto units = label(unit, "muted");
    units->setMinimumWidth(36);
    row->addWidget(units);
    body->addLayout(row);
  };
  for (int ch = 0; ch < 2; ++ch) {
    auto motor = section(QString("Motor %1").arg(ch + 1));
    readout(motor.body, "Corrente", ch, "A RMS");
    readout(motor.body, "Encoder A/B", 3 + ch, "RPM");
    auto hallRow = new QHBoxLayout;
    hallRow->addWidget(label("Hall / interno", "muted"), 1);
    auto hall = label("—");
    hall->setObjectName(QString("hallRpm%1").arg(ch + 1));
    hall->setStyleSheet("font-size:22px; font-weight:600; color:#45d4b5;");
    hall->setAlignment(Qt::AlignRight);
    hallRpm.append(hall);
    hallRow->addWidget(hall);
    hallRow->addWidget(label("RPM", "muted"));
    motor.body->addLayout(hallRow);
    auto speedSource = label("Encoder A/B: ?S · Hall/interno: ?BS", "muted");
    speedSource->setObjectName(QString("speedSource%1").arg(ch + 1));
    speedSources.append(speedSource);
    motor.body->addWidget(speedSource);
    auto flags = label("Estado: —", QString("motorFlags%1").arg(ch + 1));
    motorFlags.append(flags);
    motor.body->addWidget(flags);
    motor.body->addStretch();
    live->addWidget(motor.frame, 3);
  }
  auto fault = section("Falhas");
  faults = label("—");
  faults->setObjectName("faultSummary");
  fault.body->addWidget(label("Flags FF", "muted"));
  fault.body->addWidget(faults);
  fault.body->addWidget(label("Valor bruto do controlador", "muted"));
  fault.body->addStretch();
  live->addWidget(fault.frame, 2);
  auto power = section("Alimentação");
  readout(power.body, "Bateria", 2, "V");
  readout(power.body, "Dissipador M1", 5, "°C");
  power.body->addStretch();
  live->addWidget(power.frame, 3);
  layout->addLayout(live);
  continuousHistoryNote = label("Modo contínuo: cartões ao vivo pelo sensor interno; gráfico e I/O pausados.");
  continuousHistoryNote->setWordWrap(true);
  continuousHistoryNote->setVisible(false);
  layout->addWidget(continuousHistoryNote);
  auto command = section("Comandos de motor");
  motionEnabled = new QCheckBox("Habilitar controle manual de motor / script");
  motionEnabled->setObjectName("enableMotion");
  command.body->addWidget(motionEnabled);
  manualControlHint = label("");
  manualControlHint->setObjectName("manualControlHint");
  manualControlHint->setWordWrap(true);
  command.body->addWidget(manualControlHint);
  connect(motionEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
    if (enabled && !session.isDemo() && session.watchdogMs() < 100) {
      QSignalBlocker b(motionEnabled);
      motionEnabled->setChecked(false);
      session.enqueue("~RWD");
      notify("Consultando watchdog. Após a leitura, habilite novamente o controle manual. RWD deve ser pelo menos 100 ms.");
      return;
    }
    if (enabled && !session.isDemo() &&
        QMessageBox::question(this, "Liberar comandos",
                              "Habilitar escrita e comandos manuais de motor / script nesta sessão? "
                              "Isso desbloqueia os sliders, mas não envia movimento nem libera a parada. "
                              "Ao mover o slider ou clicar Aplicar, o comando fica mantido mesmo após soltar o mouse. Use zero/Parar para parar normalmente; PARADA !EX para emergência.",
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes) {
      QSignalBlocker b(motionEnabled);
      motionEnabled->setChecked(false);
      return;
    }
    if ((enabled && !session.setWritesEnabled(true)) ||
        !session.setMotionEnabled(enabled)) {
      QSignalBlocker b(motionEnabled);
      motionEnabled->setChecked(false);
      notify("Habilite a escrita e leia RWD na configuração. Comandos físicos "
             "exigem watchdog de pelo menos 100 ms.");
    }
    if (!enabled && session.connected())
      stopManualDrive();
    refreshState();
  });
  for (int ch = 1; ch <= 2; ++ch) {
    auto row = new QHBoxLayout;
    row->addWidget(label(QString("Motor %1").arg(ch)));
    auto mode = new ClickEdit<QComboBox>;
    mode->addItems({"G · -1000 a 1000", "S · RPM", "P · posição absoluta",
                    "GIQ · torque (A × 10)"});
    mode->setObjectName(QString("motorMode%1").arg(ch));
    row->addWidget(mode);
    auto slider = new ClickEdit<QSlider>(Qt::Horizontal);
    slider->setRange(-1000, 1000);
    slider->setObjectName(QString("motorCommand%1").arg(ch));
    row->addWidget(slider, 1);
    motorSliders.append(slider);
    auto value = new ClickEdit<QDoubleSpinBox>;
    value->setDecimals(0);
    value->setRange(-1000, 1000);
    value->setFixedWidth(100);
    value->setObjectName(QString("motorValue%1").arg(ch));
    row->addWidget(value);
    connect(slider, &QSlider::valueChanged, value, &QDoubleSpinBox::setValue);
    connect(value, &QDoubleSpinBox::valueChanged, this, [slider](double v) {
      QSignalBlocker b(slider);
      slider->setValue(int(v));
    });
    connect(mode, &QComboBox::currentIndexChanged, this,
            [slider, value](int i) {
              const int bound = i == 0   ? 1000
                                : i == 1 ? 65535
                                : i == 2 ? 4000
                                         : 450;
              slider->setRange(-bound, bound);
              value->setRange(-bound, bound);
              value->setValue(0);
            });
    auto commandState = label("Valor local: nenhum comando enviado.");
    commandState->setObjectName(QString("motorCommandStatus%1").arg(ch));
    commandState->setWordWrap(true);
    motorCommandStatus.append(commandState);
    auto repeat = new QTimer(this);
    manualRepeats.append(repeat);
    auto ackAge = std::make_shared<QElapsedTimer>();
    auto pendingCommand = std::make_shared<bool>(false);
    auto positionPhase = std::make_shared<int>(0);
    auto positionTarget = std::make_shared<qint64>(0);
    auto positionVelocity = std::make_shared<qint64>(0);
    auto apply = [this, ch, mode, value, repeat, commandState, ackAge, pendingCommand, positionPhase, positionTarget, positionVelocity] {
      if (!session.motionEnabled() || applying || manualStopping || manualFaults != 0) {
        commandState->setText("Valor local: habilite o controle e libere a parada antes de aplicar.");
        return;
      }
      const auto key = QStringList{"G", "S", "P", "GIQ"}[mode->currentIndex()];
      const auto n = qRound64(value->value());
      if (key == "G" && n != 0) {
        for (const auto &p : parameters)
          if (p.key == "MMOD" && p.channel == ch && p.known && p.raw == 3) {
            commandState->setText("G não move em posição absoluta (MMOD3). Selecione P e mova a barra para definir o destino.");
            return;
          }
      }
      if (key == "P") {
        const auto known = [this, ch](const QString &key) -> qint64 {
          for (const auto &p : parameters)
            if (p.key == key && p.channel == ch && p.known && !p.dirty) return p.raw;
          return -1;
        };
        const auto velocity = known("MVEL");
        if (known("MMOD") != 3 || velocity < 1 || velocity > 100 ||
            known("MXRPM") < 1 || known("MXRPM") > 100) {
          commandState->setText("Posição: leia os parâmetros; requer MMOD3 e MVEL/MXRPM de 1 a 100 RPM.");
          return;
        }
        *positionTarget = n;
        *positionVelocity = velocity;
      }
      if (n == 0 && key != "P") {
        repeat->stop();
        mode->setEnabled(true);
        session.enqueue(QString("!%1 %2 0").arg(key).arg(ch));
        commandState->setText("Comando zero solicitado; sem parada de emergência.");
        refreshState();
        return;
      }
      if (!repeat->isActive()) {
        ackAge->start();
        *pendingCommand = false;
        *positionPhase = 0;
        repeat->start(qBound(10, session.watchdogMs() / 3, 100));
      }
      mode->setEnabled(false);
      commandState->setText(key == "P"
          ? QString("Destino mantido: %1 contagens. Zero retorna à referência; Parar interrompe.").arg(n)
          : QString("Comando mantido: %1 %2. Soltar o mouse mantém; zero/Parar interrompe.").arg(key).arg(n));
      refreshState();
    };
    connect(&session, &Session::received, this,
            [ch, repeat, ackAge, pendingCommand, this](QString cmd, QString, bool ok) {
      const auto c = Protocol::command(cmd);
      if (repeat->isActive() && c.motion && c.channel == ch &&
          QStringList{"G", "S", "P", "GIQ"}.contains(c.key)) {
        *pendingCommand = false;
        if (ok)
          ackAge->restart();
        else
          stopManualDrive("Controlador rejeitou comando manual: " + cmd);
      }
    });
    connect(repeat, &QTimer::timeout, this, [this, ch, mode, value, repeat, ackAge, pendingCommand, positionPhase, positionTarget, positionVelocity] {
      if (!session.motionEnabled() || !session.connected()) {
        repeat->stop();
        mode->setEnabled(true);
        return;
      }
      if (applying || manualFaults != 0 || !manualFaultAge.isValid() || manualFaultAge.elapsed() > 2000) {
        stopManualDrive(applying ? "Operação incompatível com controle manual."
            : manualFaults != 0 ? QString("Falha/estado inválido do controlador: FF=%1.").arg(manualFaults)
            : "Telemetria de falhas deixou de atualizar.");
        return;
      }
      if (ackAge->elapsed() > qMax(80, session.watchdogMs() * 4 / 5)) {
        stopManualDrive("Comunicação atrasada: comando não confirmado dentro da margem do watchdog.");
        return;
      }
      // One outstanding command per channel avoids backlog and starvation
      // when both channel timers expire together during a telemetry scan.
      if (!*pendingCommand) {
        const auto key = QStringList{"G", "S", "P", "GIQ"}[mode->currentIndex()];
        *pendingCommand = true;
        QString command = QString("!%1 %2 %3").arg(key).arg(ch).arg(qRound64(value->value()));
        if (key == "P") {
          // One outstanding command; renew a fixed absolute destination, never PR.
          command = *positionPhase == 0 ? QString("!G %1 0").arg(ch)
              : *positionPhase == 1 ? QString("!S %1 %2").arg(ch).arg(*positionVelocity)
                                   : QString("!P %1 %2").arg(ch).arg(*positionTarget);
          *positionPhase = (*positionPhase + 1) % 3;
        }
        if (!session.enqueue(command))
          stopManualDrive("Comando manual recusado pela sessão.");
      }
    });
    auto send = button("Aplicar", QString("motorSend%1").arg(ch));
    motorSend.append(send);
    row->addWidget(send);
    connect(send, &QPushButton::clicked, this, apply);
    connect(slider, &QSlider::valueChanged, this, [apply, mode, slider](int) {
      // Position dragging previews the destination; release commits it once.
      // Keyboard/accessibility changes are discrete and commit immediately.
      if (mode->currentIndex() != 2 || !slider->isSliderDown())
        apply();
    });
    connect(slider, &QSlider::sliderReleased, this, [apply, mode] {
      if (mode->currentIndex() == 2)
        apply();
    });
    connect(value, &QDoubleSpinBox::editingFinished, this, apply);
    slider->setToolTip("G/S/GIQ: mover a barra aplica e mantém o comando, mesmo após soltar. Zero ou Parar para parada normal. Posição: -4000 a 4000 contagens; soltar a barra confirma o destino, que fica mantido. Aplicar também confirma. Zero retorna à referência; Parar interrompe.");
    auto stop = button("Parar", QString("motorStop%1").arg(ch));
    motorStop.append(stop);
    row->addWidget(stop);
    connect(stop, &QPushButton::clicked, this, [this, ch, mode, value, repeat, apply] {
      repeat->stop();
      mode->setEnabled(true);
      value->setValue(0);
      if (mode->currentIndex() == 2) {
        if (!session.enqueue(QString("!MS %1").arg(ch)))
          notify("Parada de posição recusada pela sessão.");
      } else
        apply();
    });
    command.body->addLayout(row);
    command.body->addWidget(commandState);
  }
  auto safety = new QHBoxLayout;
  auto estop = button("PARADA · !EX", "emergencyStop");
  auto release = button("Liberar parada / falhas", "releaseEmergency");
  safety->addWidget(estop);
  safety->addWidget(release);
  command.body->addLayout(safety);
  liveActions.append(estop);
  liveActions.append(release);
  estop->setStyleSheet("background:#8b2833;color:white;font-weight:700");
  connect(estop, &QPushButton::clicked, this, [this] {
    if (applying)
      completeApply(false,
                    "Operação cancelada pela parada; releia a configuração.");
    session.enqueue("!EX");
    notify("Parada solicitada. Confira as flags do controlador.");
    refreshState();
  });
  connect(release, &QPushButton::clicked, this, [this] {
    runtime("!MG", "Liberar a parada de emergência e limpar falhas. Os motores "
                   "poderão responder aos comandos existentes.");
  });
  layout->addWidget(command.frame);
  auto lower = new QSplitter(Qt::Horizontal);
  lower->setChildrenCollapsible(false);
  auto graph = section("Gráfico");
  auto metric = new ClickEdit<QComboBox>;
  metric->setObjectName("monitorMetric");
  metric->addItems(
      {"Corrente / A", "Encoder / RPM", "Bateria / V", "Dissipador / °C", "Hall / RPM"});
  graph.tools->addWidget(metric);
  auto save = button("CSV", "exportCsv");
  graph.tools->addWidget(save);
  auto clear = button("Limpar gráfico");
  graph.tools->addWidget(clear);
  chart = new Chart;
  graph.body->addWidget(chart, 1);
  connect(metric, &QComboBox::currentIndexChanged, this,
          [this](int i) { chart->setMetric(i); });
  connect(save, &QPushButton::clicked, this, &Window::exportTelemetry);
  connect(clear, &QPushButton::clicked, this, [this] { chart->clear(); });
  auto feed = section("Sinais / extremos da sessão");
  chartFeed = signalTable("chartFeed");
  feed.body->addWidget(chartFeed, 1);
  ioValues = new QTableWidget(10, 4);
  ioValues->setObjectName("ioValues");
  ioValues->setHorizontalHeaderLabels({"I/O", "Digital", "Analog", "Pulso"});
  ioValues->verticalHeader()->hide();
  ioValues->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ioValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 4; ++j)
      ioValues->setItem(
          i, j, new QTableWidgetItem(j == 0 ? QString::number(i + 1) : "—"));
  digitalOutputs = label("Saídas D-Out: —", "digitalOutputs");
  feed.body->addWidget(digitalOutputs);
  ioValues->setMinimumHeight(150);
  feed.body->addWidget(ioValues, 1);
  feed.body->addWidget(label("Atualização nominal: 500 ms\nJanela do gráfico: "
                             "60 s\n● M1 azul   ● M2 verde",
                             "muted"));
  feed.frame->setMinimumWidth(350);
  lower->addWidget(graph.frame);
  lower->addWidget(feed.frame);
  lower->setSizes({1000, 360});
  layout->addWidget(lower, 1);
  auto scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  page->setObjectName("scrollContent");
  scroll->setWidget(page);
  return scroll;
}

QWidget *Window::consolePage() {
  auto page = new QWidget;
  auto layout = new QHBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto side = new QVBoxLayout;
  side->setSpacing(6);
  auto queries = section("Consultas");
  query = new QLineEdit;
  query->setObjectName("queryInput");
  query->setPlaceholderText("?FID");
  sendQuery = button("Consultar", "sendQuery");
  queries.body->addWidget(query);
  queries.body->addWidget(sendQuery);
  auto presets = new QListWidget;
  presets->setObjectName("queryPresets");
  for (const auto &cmd : {"?FID", "?TRN", "?A", "?S", "?V 2", "?T 2", "?C",
                          "?FF", "?FS", "?DI", "?DO"})
    presets->addItem(cmd);
  queries.body->addWidget(presets, 1);
  queries.body->addWidget(label(
      "Somente consultas. Comandos de movimento e escrita são bloqueados.",
      "muted"));
  side->addWidget(queries.frame, 1);
  auto maintenance = section("Manutenção");
  auto dfu = button("Atualizar firmware via USB", "firmwareUSB");
  maintenance.body->addWidget(dfu);
  liveActions.append(dfu);
  connect(dfu, &QPushButton::clicked, this, [this] { showFirmwareUpdate(); });
  auto can = button("CANopen / SocketCAN", "canInspector");
  maintenance.body->addWidget(can);
  connect(can, &QPushButton::clicked, this, &Window::showCanInspector);
  maintenance.body->addWidget(unavailable("Atualizar firmware via serial"));
  maintenance.body->addWidget(unavailable("Atualizar firmware via CANopen"));
  maintenance.body->addWidget(
      label("USB nativo · DFU oficial por modelo", "muted"));
  side->addWidget(maintenance.frame);
  auto commandBox = section("Comando documentado");
  auto commandInput = new QLineEdit;
  commandInput->setObjectName("commandInput");
  commandInput->setPlaceholderText("Ex.: !MS 1 ou ^ALIM 1 150");
  auto execute = button("Revisar / executar", "executeCommand");
  commandBox.body->addWidget(commandInput);
  commandBox.body->addWidget(execute);
  commandBox.body->addWidget(label("Escritas usam a revisão e verificação das "
                                   "abas de configuração/manutenção.",
                                   "muted"));
  connect(execute, &QPushButton::clicked, this, [this, commandInput] {
    executeDocumentedCommand(commandInput->text());
  });
  side->addWidget(commandBox.frame);
  auto sidebar = new QWidget;
  sidebar->setLayout(side);
  sidebar->setObjectName("scrollContent");
  side->setContentsMargins(0, 0, 0, 0);
  auto sidebarScroll = new QScrollArea;
  sidebarScroll->setWidgetResizable(true);
  sidebarScroll->setFrameShape(QFrame::NoFrame);
  sidebarScroll->setFixedWidth(280);
  sidebarScroll->setWidget(sidebar);
  layout->addWidget(sidebarScroll);
  auto terminal = section("Console");
  auto clear = button("Limpar");
  terminal.tools->addWidget(clear);
  auto save = button("Salvar log");
  terminal.tools->addWidget(save);
  connect(save, &QPushButton::clicked, this, &Window::saveLog);
  console = new QPlainTextEdit;
  console->setObjectName("trafficConsole");
  console->setReadOnly(true);
  console->setMaximumBlockCount(2000);
  terminal.body->addWidget(console, 1);
  layout->addWidget(terminal.frame, 1);
  auto send = [this] {
    const QString text = query->text();
    const auto c = Protocol::command(text);
    if (!c.valid || c.write) {
      notify("Consulta não permitida. O console aceita apenas leituras da "
             "lista suportada.");
      return;
    }
    if (!session.enqueue(text))
      notify("Conecte uma fonte e aguarde a operação atual.");
    else
      query->clear();
  };
  connect(sendQuery, &QPushButton::clicked, this, send);
  connect(query, &QLineEdit::returnPressed, this, send);
  connect(presets, &QListWidget::currentTextChanged, query,
          &QLineEdit::setText);
  connect(presets, &QListWidget::itemDoubleClicked, this,
          [this] { sendQuery->click(); });
  connect(clear, &QPushButton::clicked, console, &QPlainTextEdit::clear);
  return page;
}

QWidget *Window::diagnosticsPage() {
  auto page = new QWidget;
  auto layout = new QHBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto side = new QWidget;
  side->setMinimumWidth(350);
  auto controls = new QVBoxLayout(side);
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(6);
  auto options = section("Opções");
  auto metric = new ClickEdit<QComboBox>;
  metric->setObjectName("diagnosticMetric");
  metric->addItems(
      {"Corrente / A", "Encoder / RPM", "Bateria / V", "Dissipador / °C", "Hall / RPM"});
  options.body->addWidget(metric);
  streamRate = new ClickEdit<QComboBox>;
  streamRate->setObjectName("streamRate");
  for (int interval : {1, 2, 5, 10, 20, 50, 100, 500})
    streamRate->addItem(QString("%1 ms").arg(interval), interval);
  streamRate->setCurrentIndex(5);
  options.body->addWidget(streamRate);
  streamButton = button("Iniciar captura contínua", "startStreaming");
  options.body->addWidget(streamButton);
  connect(streamButton, &QPushButton::clicked, this, &Window::toggleStreaming);
  options.body->addWidget(label("1-10 ms: USB CDC. Serial física: mínimo de 20 "
                                "ms. O CSV registra o instante de recepção.",
                                "muted"));
  controls->addWidget(options.frame);
  auto plots = section("Sinais");
  diagnosticFeed = signalTable("diagnosticFeed");
  plots.body->addWidget(diagnosticFeed, 1);
  auto save = button("Exportar CSV");
  plots.body->addWidget(save);
  controls->addWidget(plots.frame, 1);
  auto configure = section("Motor / sensores");
  configure.body->addWidget(unavailable("Caracterização do motor"));
  auto sensors = button("Configurar motor / sensores");
  configure.body->addWidget(sensors);
  connect(sensors, &QPushButton::clicked, this, &Window::showSensorSetup);
  configure.body->addWidget(unavailable("Correção de linearidade"));
  configure.body->addWidget(unavailable("Ajuste automático de malhas"));
  configure.body->addWidget(
      label("Assistentes ainda não implementados", "muted"));
  controls->addWidget(configure.frame);
  auto area = new QWidget;
  auto display = new QVBoxLayout(area);
  display->setContentsMargins(0, 0, 0, 0);
  display->setSpacing(6);
  auto plot = section("Gráfico de diagnóstico");
  auto clear = button("Limpar gráfico");
  plot.tools->addWidget(clear);
  diagnosticChart = new Chart;
  diagnosticChart->setObjectName("diagnosticChart");
  plot.body->addWidget(diagnosticChart, 1);
  display->addWidget(plot.frame, 3);
  auto log = section("Log de diagnóstico / protocolo");
  auto clearLog = button("Limpar log");
  log.tools->addWidget(clearLog);
  diagnosticLog = new QPlainTextEdit;
  diagnosticLog->setObjectName("diagnosticLog");
  diagnosticLog->setReadOnly(true);
  diagnosticLog->setMaximumBlockCount(2000);
  log.body->addWidget(diagnosticLog, 1);
  display->addWidget(log.frame, 1);
  auto split = new QSplitter(Qt::Horizontal);
  split->setChildrenCollapsible(false);
  split->addWidget(side);
  split->addWidget(area);
  split->setSizes({360, 1000});
  layout->addWidget(split);
  connect(metric, &QComboBox::currentIndexChanged, this,
          [this](int i) { diagnosticChart->setMetric(i); });
  connect(clear, &QPushButton::clicked, this,
          [this] { diagnosticChart->clear(); });
  connect(clearLog, &QPushButton::clicked, diagnosticLog,
          &QPlainTextEdit::clear);
  connect(save, &QPushButton::clicked, this, &Window::exportTelemetry);
  return page;
}

QWidget *Window::scriptsPage() {
  auto page = new QWidget;
  auto layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto toolbar = new QFrame;
  toolbar->setObjectName("panelHeader");
  auto tools = new QHBoxLayout(toolbar);
  tools->setContentsMargins(6, 5, 6, 5);
  tools->setSpacing(5);
  auto open = button("Abrir .mbs");
  auto save = button("Salvar .mbs");
  tools->addWidget(open);
  tools->addWidget(save);
  auto undo = button("↶");
  undo->setToolTip("Desfazer");
  tools->addWidget(undo);
  auto redo = button("↷");
  redo->setToolTip("Refazer");
  tools->addWidget(redo);
  tools->addSpacing(15);
  auto validate = button("Compilar / HEX", "buildScript");
  tools->addWidget(validate);
  connect(validate, &QPushButton::clicked, this, [this] { compileScript(); });
  auto bytecodeSim = button("Simular bytecode", "simulateBytecode");
  tools->addWidget(bytecodeSim);
  connect(bytecodeSim, &QPushButton::clicked, this,
          [this] { compileScript(true); });
  auto simulate = button("Depurar fonte", "simulateScript");
  tools->addWidget(simulate);
  connect(simulate, &QPushButton::clicked, this, &Window::showScriptSimulator);
  auto upload = button("Enviar HEX", "uploadScript");
  tools->addWidget(upload);
  liveActions.append(upload);
  connect(upload, &QPushButton::clicked, this, &Window::scriptUpload);
  tools->addStretch();
  auto editorCaption = label("MICROBasic  ·  editor local", "panelTitle");
  editorCaption->setWordWrap(false);
  tools->addWidget(editorCaption);
  layout->addWidget(toolbar);
  script = new ScriptEditor;
  script->setObjectName("scriptEditor");
  script->setTabStopDistance(32);
  script->setPlainText(
      "' Exemplo para edição. Não é enviado nem executado.\nOption "
      "Explicit\n\nDim corrente As Integer\n\nWhile True\n    corrente = "
      "GetValue(_A, 1)\n    Print(\"Corrente canal 1: \", corrente, \"\\n\")\n "
      "   Wait(500)\nEnd While\n");
  new BasicHighlighter(script->document());
  layout->addWidget(script, 1);
  layout->addWidget(
      label("Compilador e simulador de bytecode recuperados do RoboRun+ 3.3. "
            "Depuração da fonte usa o simulador parcial do MotorLab. "
            "Exportar HEX não envia nem executa no controlador.",
            "muted"));
  connect(undo, &QPushButton::clicked, script, &QPlainTextEdit::undo);
  connect(redo, &QPushButton::clicked, script, &QPlainTextEdit::redo);
  connect(open, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getOpenFileName(
        this, "Abrir script", QString(), "MicroBasic (*.mbs);;Texto (*.txt)");
    if (path.isEmpty())
      return;
    if (script->document()->isModified() &&
        QMessageBox::question(
            this, "Substituir rascunho",
            "Descartar as alterações não salvas do editor?") !=
            QMessageBox::Yes)
      return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) {
      notify("Não foi possível abrir o script (limite: 1 MiB).");
      return;
    }
    script->setPlainText(QString::fromUtf8(f.readAll()));
    script->document()->setModified(false);
    notify("Script aberto: " + path);
  });
  connect(save, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getSaveFileName(
        this, "Salvar script", "script.mbs", "MicroBasic (*.mbs)");
    if (path.isEmpty())
      return;
    QSaveFile f(path);
    const auto data = script->toPlainText().toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() ||
        !f.commit()) {
      notify("Falha ao salvar script.");
      return;
    }
    script->document()->setModified(false);
    notify("Script salvo: " + path);
  });
  return page;
}

QWidget *Window::aboutPage() {
  auto page = new QWidget;
  auto layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto identity = section("Identificação");
  identity.body->addWidget(label("MotorLab Linux 0.6.0"));
  identity.body->addWidget(
      label("Aplicativo independente, não oficial da Roboteq.", "muted"));
  device = label("Modelo detectado: —");
  firmware = label("Firmware detectado: —");
  identity.body->addWidget(device);
  identity.body->addWidget(firmware);
  identity.body->addWidget(label(
      "Referência: SBLG2360T / SBLG2XXX v3.0f e SBLMG2360T / SBLMG2XXX v3.0f.",
      "muted"));
  layout->addWidget(identity.frame);
  auto coverage = section("Recursos desta versão");
  coverage.body->addWidget(
      label("Conexão USB/RS232, identidade e telemetria; gráficos, log e CSV; "
            "árvores originais, perfis JSON/CPR e comparação; edição na RAM "
            "com confirmação e releitura; EEPROM, comandos de motor, captura "
            "contínua, compilação MicroBasic, simulação de bytecode, carga "
            "HEX, DFU USB e ajuste de "
            "sensores; demonstração identificada."));
  coverage.body->addWidget(label(
      "Pendências: firmware serial/CAN, "
      "escrita/movimento por CAN, caracterização, correção de linearidade "
      "e ajuste automático PI. "
      "Perfil cobre o catálogo "
      "documentado; campos rejeitados ficam indisponíveis.",
      "muted"));
  coverage.body->addWidget(
      label("Interface organizada a partir das referências do RoboRun+, "
            "mantendo a paleta escura do MotorLab. A organização visual não "
            "acrescenta funções ao controlador.",
            "muted"));
  layout->addWidget(coverage.frame);
  layout->addStretch();
  return page;
}

void Window::selectPage(int index) {
  if (index < 0 || index >= pages->count())
    return;
  pages->setCurrentIndex(index);
  if (navigation->currentIndex() != index)
    navigation->setCurrentIndex(index);
}

void Window::exportTelemetry() {
  if (recorded.isEmpty()) {
    notify("Ainda não há amostras para exportar.");
    return;
  }
  const auto path = QFileDialog::getSaveFileName(
      this, "Exportar telemetria", "telemetria.csv", "CSV (*.csv)");
  if (!path.isEmpty())
    notify(saveCsv(path) ? "Telemetria exportada: " + path
                         : "Não foi possível salvar o CSV.");
}

void Window::startDemo() {
  source->setCurrentIndex(0);
  if (session.connected())
    session.close();
  connectSource();
}
void Window::connectSource() {
  if (session.connected()) {
    session.close();
    notify(
        "Conexão encerrada. O gráfico mantém o histórico da sessão anterior.");
    return;
  }
  scanActive = false;
  readings.clear();
  recorded.clear();
  chart->clear();
  diagnosticChart->clear();
  console->clear();
  diagnosticLog->clear();
  elapsed.start();
  minimums.fill(qQNaN());
  maximums.fill(qQNaN());
  for (auto feed : {chartFeed, diagnosticFeed})
    for (int row = 0; row < 6; ++row)
      for (int column = 1; column < 4; ++column)
        feed->item(row, column)->setText("—");
  captureDemo = source->currentIndex() == 0;
  captureFirmware.clear();
  captureModel.clear();
  profileModel.clear();
  profileFirmware.clear();
  profileSource = captureDemo ? "demo" : "serial";
  for (auto &p : parameters) {
    p.known = false;
    p.dirty = false;
  }
  if (captureDemo) {
    session.openDemo();
    notify(
        "DEMONSTRAÇÃO: telemetria e alterações de parâmetros são simuladas.");
  } else if (session.openSerial(port->currentText().trimmed(),
                                baud->currentText().toInt()))
    notify("Consultando modelo e firmware; edição permanece desabilitada.");
}

void Window::refreshPorts() {
  const QString selected = port->currentText();
  port->clear();
  QDir byId("/dev/serial/by-id");
  for (const auto &name :
       byId.entryList(QDir::Files | QDir::System | QDir::NoDotAndDotDot))
    port->addItem(byId.absoluteFilePath(name));
  QDir dev("/dev");
  for (const auto &name :
       dev.entryList({"ttyACM*", "ttyUSB*"}, QDir::System | QDir::Files))
    port->addItem(dev.absoluteFilePath(name));
  if (!selected.isEmpty())
    port->setEditText(selected);
  refreshState();
}

void Window::refreshState() {
  const bool live = session.connected(), demo = session.isDemo();
  source->setEnabled(!live);
  port->setEnabled(!live && source->currentIndex() == 1);
  baud->setEnabled(!live && source->currentIndex() == 1);
  portRefresh->setEnabled(!live);
  connection->setText(live                          ? "Desconectar"
                      : source->currentIndex() == 0 ? "Abrir demo"
                                                    : "Conectar");
  connection->setEnabled(!applying);
  status->setText(!live  ? captureDemo && !recorded.isEmpty()
                               ? "DESCONECTADO • HISTÓRICO SIMULADO"
                               : "DESCONECTADO"
                  : demo ? "DEMONSTRAÇÃO • DADOS SIMULADOS"
                  : session.writesEnabled() ? "USB/SERIAL • EDIÇÃO RAM"
                                            : "USB/SERIAL • LEITURA");
  device->setText("Modelo detectado: " +
                  (session.model().isEmpty() ? "—" : session.model()));
  const QString identified = Protocol::supportedModel(session.model());
  detectedModel->setText("Detectado: " +
                         (session.model().isEmpty() ? QStringLiteral("—")
                          : identified.isEmpty()    ? session.model()
                                                    : identified));
  firmware->setText("Firmware detectado: " +
                    (session.firmware().isEmpty() ? "—" : session.firmware()));
  const QString footerVersion =
      session.firmware().isEmpty() ? QStringLiteral("—") : session.firmware();
  footerFirmware->setText("Firmware: " + footerVersion);
  footerFirmware->setToolTip(footerVersion);
  const bool canEdit =
      !live || demo ||
      Protocol::compatible(session.firmware(), session.model());
  offlineModel->setEnabled(!live);
  editEnabled->setEnabled(canEdit && !applying && readsPending == 0);
  {
    const QSignalBlocker b(editEnabled);
    editEnabled->setChecked(session.writesEnabled());
  }
  readParams->setEnabled(live && !applying && !manualDriveActive() && readsPending == 0);
  snapshot->setEnabled(!applying && readsPending == 0);
  applyParams->setEnabled(live && session.writesEnabled() && !applying && !manualDriveActive() &&
                          readsPending == 0);
  query->setEnabled(live && !applying);
  sendQuery->setEnabled(live && !applying);
  for (int i = 0; i < parameters.size(); ++i) {
    auto &p = parameters[i];

    if (p.editor) {
      const bool enabled = p.editable &&
          (!live || (p.known && session.writesEnabled() &&
                     p.raw >= p.spec->minimum && p.raw <= p.spec->maximum)) &&
          !applying && readsPending == 0;
      // The hidden numeric editor is a child of the combo. Its effective
      // isEnabled() inherits the combo's old state and cannot unlock it.
      if (p.choices)
        p.choices->setEnabled(enabled);
      p.editor->setEnabled(enabled);
    }
    updateParameterRow(i);
  }
  const bool authorized =
      live &&
      (demo || Protocol::compatible(session.firmware(), session.model()));
  if (motionEnabled) {
    motionEnabled->setEnabled(authorized && !applying && !manualStopping && readsPending == 0);
    {
      QSignalBlocker b(motionEnabled);
      motionEnabled->setChecked(session.motionEnabled());
    }
  }
  for (auto action : liveActions)
    action->setEnabled(authorized &&
                       (!applying || action->objectName() == "emergencyStop" ||
                        action->objectName() == "pauseScript"));
  if (auto dfu = findChild<QPushButton *>("firmwareUSB"))
    dfu->setEnabled(live && !demo && !identified.isEmpty() &&
                    !session.usbPath().isEmpty() && !applying && !readsPending);
  for (auto widget : motorSliders)
    widget->setEnabled(session.motionEnabled() && !applying);
  for (auto widget : motorSend)
    widget->setEnabled(session.motionEnabled() && !applying);
  refreshManualControls();
  for (int i = 0; i < manualRepeats.size(); ++i)
    findChild<QComboBox *>(QString("motorMode%1").arg(i + 1))->setEnabled(!manualRepeats[i]->isActive());
  for (auto widget : motorStop)
    widget->setEnabled(authorized);
  if (streamButton) {
    streamButton->setEnabled(authorized && !applying && readsPending == 0);
    streamButton->setText(session.streaming() ? "Parar captura contínua"
                                              : "Iniciar captura contínua");
    streamRate->setEnabled(!session.streaming());
  }
  if (!live) {
    if (!profileModel.isEmpty())
      readProgress->setText("Valores históricos / rascunho offline");
    operationExpected.clear();
    operationTarget.clear();
    applying = false;
    scanActive = false;
    readsPending = 0;
    readings.clear();
    for (auto l : metrics)
      l->setText("—");
    for (auto feed : {chartFeed, diagnosticFeed})
      for (int row = 0; row < 6; ++row)
        feed->item(row, 1)->setText("—");
    if (ioValues)
      for (int i = 0; i < 10; ++i)
        for (int j = 1; j < 4; ++j)
          ioValues->item(i, j)->setText("—");
    for (auto flags : motorFlags)
      flags->setText("Estado: —");
    if (digitalOutputs)
      digitalOutputs->setText("Saídas D-Out: —");
    for (auto value : hallRpm) value->setText("—");
    encoders->setText("—");
    faults->setText("—");
    faults->setStyleSheet("");
    motorAlarmSummary.clear();
    statusFlags->setText("—");
  }
}

void Window::refreshManualControls() {
  if (!manualControlHint)
    return;
  if (!session.connected())
    manualFaults = -1;
  if (!session.motionEnabled())
    for (auto timer : manualRepeats)
      timer->stop();
  if (auto release = findChild<QPushButton *>("releaseEmergency"))
    release->setEnabled(session.motionEnabled() && !applying && !manualStopping && (manualFaults & 16) && manualFaults >= 0);
  const bool ready = session.connected() && session.motionEnabled() &&
                     !applying && !manualStopping && manualFaults == 0;
  for (auto widget : motorSend)
    widget->setEnabled(ready);
  manualControlHint->setText(manualStopping
      ? "Parando: aguardando a resposta em andamento para transmitir !EX."
      : applying
      ? "Operação em andamento: controle manual indisponível. PARADA · !EX continua disponível."
      : !session.connected() ? "Conecte o controlador para habilitar o controle manual."
      : !session.motionEnabled() ? "1. Habilite o controle manual. 2. Se a parada estiver ativa, use Liberar parada / falhas. 3. Mova a barra: o comando permanece após soltar; zero/Parar interrompe."
      : manualFaults < 0 ? "Aguardando leitura das falhas antes de transmitir comandos."
      : (manualFaults & 16) ? "PARADA ATIVA: o valor do slider ainda não move o motor. Use Liberar parada / falhas e confirme; depois mova a barra. O comando permanece após soltar."
      : manualFaults != 0 ? "Controlador com falha: envio manual bloqueado. Confira o painel Falhas."
      : !motorAlarmSummary.isEmpty() ? motorAlarmSummary + " — confira o painel Falhas. Zere o comando antes de retomar o teste."
      : "PRONTO: mover a barra aplica e mantém o comando. Em P, soltar confirma o destino absoluto; zero retorna à referência e Parar interrompe. G/S: zero ou Parar = parada normal. Botão vermelho = emergência.");
}

void Window::updateFaultSummary(int ff, int fm1, int fm2) {
  QStringList alerts, limits;
  for (int ch = 0; ch < 2; ++ch) {
    const int flags = ch == 0 ? fm1 : fm2;
    if (flags < 0) continue;
    if (flags & 1) limits << QString("Motor %1: AmpLim — limitação de corrente ativa").arg(ch + 1);
    const int alarms = flags & 0x7e; // AmpLim is regulation; FETs Off alone is a state.
    if (!alarms) continue;
    QString names = Protocol::motorNames(alarms).join(" · ");
    if (alarms & 4) names.replace("LoopErr", "LoopErr: erro de acompanhamento");
    alerts << QString("Motor %1: %2").arg(ch + 1).arg(names);
  }
  motorAlarmSummary = alerts.join("; ");
  QString text = ff < 0 ? "Falhas gerais: aguardando leitura"
                       : "Falhas gerais: " + Protocol::faultNames(ff).join(" · ");
  if (!alerts.isEmpty()) text += "\n" + alerts.join("\n");
  if (!limits.isEmpty()) text += "\n" + limits.join("\n");
  faults->setText(text);
  faults->setWordWrap(true);
  faults->setStyleSheet(ff > 0 || !alerts.isEmpty()
      ? "color: #ffb4b4; background: #48232b; padding: 8px; font-weight: bold;"
      : !limits.isEmpty() ? "color: #ffd58a; padding: 8px;" : "");
  refreshManualControls();
}

bool Window::manualDriveActive() const {
  if (!session.motionEnabled())
    return false;
  return std::any_of(manualRepeats.begin(), manualRepeats.end(), [](auto t) { return t->isActive(); });
}

void Window::stopManualDrive(const QString &reason) {
  if (manualStopping)
    return;
  manualStopping = true;
  for (auto timer : manualRepeats)
    timer->stop();
  notify(reason);
  writeLog("PARADA MANUAL", reason);
  session.setMotionEnabled(false); // Remove queued motor commands immediately.
  refreshState();
  auto timer = new QTimer(this);
  auto age = std::make_shared<QElapsedTimer>();
  age->start();
  connect(timer, &QTimer::timeout, this, [this, timer, age] {
    // Normally drain the current telemetry scan. If transport stalls, the
    // existing priority EX path sends immediately and closes the session.
    if (session.connected() && session.busy() && age->elapsed() < 250)
      return;
    timer->stop();
    timer->deleteLater();
    if (session.connected())
      session.enqueue("!EX");
    manualStopping = false;
    refreshState();
  });
  timer->start(10);
}

void Window::poll() {
  if (!session.connected() || session.streaming() || session.uploading() ||
      session.busy() || applying || readsPending || scanActive ||
      session.model().isEmpty())
    return;
  readings.clear();
  scanActive = true;
  for (const auto &cmd : Protocol::telemetryCommands())
    if (!session.enqueue(cmd)) {
      scanActive = false;
      break;
    }
}

void Window::onReply(const QString &cmd, const QString &payload, bool ok) {
  if (cmd == "?FF") {
    const auto values = integers(payload);
    manualFaultAge.restart();
    manualFaults = ok && values.size() == 1 && values[0] >= 0 && values[0] <= 65535
        ? int(values[0]) : -1;
    refreshManualControls();
  }
  if (cmd == "!MG" && ok) {
    manualFaults = -1;
    session.enqueue("?FF");
    refreshManualControls();
  }
  const auto manualCommand = Protocol::command(cmd);
  if (manualCommand.motion && manualCommand.channel >= 1 && manualCommand.channel <= motorCommandStatus.size())
    motorCommandStatus[manualCommand.channel - 1]->setText(cmd +
        (ok ? " — aceito pelo controlador (ACK)." : " — REJEITADO: " + payload));
  if (cmd == "!EX") {
    for (auto state : motorCommandStatus)
      state->setText(ok ? "Parada !EX aceita. Reabilite o controle para outro acionamento."
                        : "Parada não confirmada: " + payload);
    refreshState();
  }
  if (!operationExpected.isEmpty())
    maintenanceReply(cmd, payload, ok);
  if (cmd == "?FID") {
    captureFirmware = ok ? payload : QString();
    profileFirmware = captureFirmware;
  }
  if (cmd == "?TRN") {
    captureModel = ok ? payload : QString();
    profileModel = Protocol::supportedModel(captureModel);
    if (!profileModel.isEmpty()) {
      QSignalBlocker block(offlineModel);
      offlineModel->setCurrentText(profileModel);
    }
    if (ok) {
      // Read only the manual-control prerequisites at connection time.
      // Never change the controller mode or activate a motor on startup.
      session.enqueue("~RWD");
      for (int ch = 1; ch <= 2; ++ch)
        for (const auto &key : {"MMOD", "MVEL", "MXRPM"})
          session.enqueue(QString("~%1 %2").arg(key).arg(ch));
    }
  }
  if (cmd == "?TRN" && ok && !session.isDemo()) {
    notify(Protocol::compatible(session.firmware(), session.model())
               ? Protocol::supportedModel(payload) +
                     " / v3.0f identificado. Leitura ativa; habilite a edição "
                     "na tela de parâmetros quando necessário."
               : "Leitura ativa. Modelo/firmware diferente da referência: "
                 "edição de parâmetros indisponível nesta versão.");
  }
  if (cmd.startsWith('~')) {
    const QString key = cmd.mid(1);
    for (int row = 0; row < parameters.size(); ++row) {
      auto &p = parameters[row];
      if (key != p.suffix)
        continue;
      const auto numbers = integers(payload);
      p.known = ok && numbers.size() == 1;
      if (p.known)
        p.raw = numbers.first();
      if (p.editor && p.known) {
        const double v = p.raw / p.scale;
        const bool inRange =
            v >= p.editor->minimum() && v <= p.editor->maximum();
        if (inRange && (!p.dirty || applying)) {
          const QSignalBlocker blocker(p.editor);
          p.editor->setValue(v);
          p.dirty = false;
        }
      }
      updateParameterRow(row);
      if (p.key == "PMOD" || p.key == "MMOD")
        for (int i = 0; i < parameters.size(); ++i)
          if (parameters[i].channel == p.channel)
            updateParameterRow(i);
      if (p.key == "MMOD" && p.known && !p.dirty && p.raw == 3 &&
          p.channel >= 1 && p.channel <= manualRepeats.size() &&
          !manualRepeats[p.channel - 1]->isActive()) {
        auto mode = findChild<QComboBox *>(QString("motorMode%1").arg(p.channel));
        mode->setCurrentIndex(2);
      }
    }
    if (readsPending > 0 && nextParameter > 0 &&
        cmd == "~" + parameters[nextParameter - 1].suffix) {
      --readsPending;
      readNextParameter();
    }
  }
  if (applying) {
    const auto change = changes.value(applyIndex, QPair<int, qint64>{-1, -1});
    if (change.first < 0)
      return;
    const auto &p = parameters[change.first];
    const QString suffix = p.suffix;
    if (!verifyWrite &&
        cmd == "^" + suffix + " " + QString::number(change.second)) {
      if (!ok) {
        completeApply(false, "Controlador rejeitou a alteração. Releia os "
                             "valores antes de continuar.");
        return;
      }
      verifyWrite = true;
      if (!session.enqueue("~" + suffix))
        completeApply(false, "Não foi possível solicitar a verificação.");
    } else if (verifyWrite && cmd == "~" + suffix) {
      const auto n = integers(payload);
      if (!ok || n.size() != 1 || n.first() != change.second) {
        completeApply(false, "A releitura não confirmou o valor solicitado. "
                             "Algumas alterações podem ter sido aplicadas.");
        return;
      }
      ++applyIndex;
      verifyWrite = false;
      applyNext();
    }
  }
  if (scanActive && Protocol::telemetryCommands().contains(cmd)) {
    if (ok)
      readings[cmd] = payload;
    else
      readings.remove(cmd);
    if (cmd == "?FS") {
      scanActive = false;
      updateSamples();
    }
  }
}

void Window::updateSamples() {
  Sample s;
  s.seconds = elapsed.elapsed() / 1000.0;
  s.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  const auto a = integers(readings.value("?A")),
             speed = integers(readings.value("?S"));
  const auto v = integers(readings.value("?V 2")),
             t = integers(readings.value("?T 2"));
  const auto c = integers(readings.value("?C")),
             ff = integers(readings.value("?FF")),
             fs = integers(readings.value("?FS"));
  if (a.size() == 2) {
    s.amp1 = a[0] / 10.0;
    s.amp2 = a[1] / 10.0;
  }
  if (speed.size() == 2) {
    s.rpm1 = speed[0];
    s.rpm2 = speed[1];
  }
  if (v.size() == 1)
    s.voltage = v[0] / 10.0;
  if (t.size() == 1)
    s.temperature = t[0];
  if (c.size() == 2) {
    s.encoder1 = c[0];
    s.encoder2 = c[1];
    s.hasEncoder = true;
  }
  if (ff.size() == 1) {
    s.faults = ff[0];
    s.hasFaults = true;
  }
  manualFaultAge.restart();
  manualFaults = ff.size() == 1 && ff[0] >= 0 && ff[0] <= 65535
      ? int(ff[0]) : -1;
  refreshManualControls();
  if (fs.size() == 1) {
    s.flags = fs[0];
    s.hasFlags = true;
  }
  metrics[0]->setText(number(s.amp1));
  metrics[1]->setText(number(s.amp2));
  metrics[2]->setText(number(s.voltage));
  metrics[3]->setText(number(s.rpm1, 0));
  metrics[4]->setText(number(s.rpm2, 0));
  metrics[5]->setText(number(s.temperature, 0));
  const auto h1 = integers(readings.value("?BS 1"));
  const auto h2 = integers(readings.value("?BS 2"));
  if (h1.size() == 1) s.hallRpm1 = h1[0];
  if (h2.size() == 1) s.hallRpm2 = h2[0];
  hallRpm[0]->setText(number(s.hallRpm1, 0));
  hallRpm[1]->setText(number(s.hallRpm2, 0));
  encoders->setText(
      s.hasEncoder
          ? QString("C1: %1   ·   C2: %2").arg(s.encoder1).arg(s.encoder2)
          : "Indisponível");
  const auto fm1 = integers(readings.value("?FM 1"));
  const auto fm2 = integers(readings.value("?FM 2"));
  updateFaultSummary(s.hasFaults ? s.faults : -1,
                     fm1.size() == 1 ? int(fm1[0]) : -1,
                     fm2.size() == 1 ? int(fm2[0]) : -1);
  statusFlags->setText(s.hasFlags ? Protocol::statusNames(s.flags).join(" · ") +
                                        QString(" (0x%1)").arg(s.flags, 0, 16)
                                  : "Indisponível");
  if (ioValues) {
    const auto di = integers(readings.value("?DI"));
    for (int i = 0; i < 10; ++i) {
      ioValues->item(i, 1)->setText(
          di.size() == 1 ? QString::number((di[0] >> i) & 1) : "—");
      auto a = integers(readings.value(QString("?AI %1").arg(i + 1))),
           p = integers(readings.value(QString("?PI %1").arg(i + 1)));
      ioValues->item(i, 2)->setText(a.size() == 1 ? QString::number(a[0])
                                                  : "—");
      ioValues->item(i, 3)->setText(p.size() == 1 ? QString::number(p[0])
                                                  : "—");
    }
    const auto out = integers(readings.value("?DO"));
    QStringList outs;
    for (int i = 0; i < 4; ++i)
      outs << QString("D%1: %2").arg(i + 1).arg(
          out.size() == 1 ? QString::number((out[0] >> i) & 1) : "—");
    digitalOutputs->setText(outs.join(" · "));
    for (int ch = 0; ch < 2; ++ch) {
      auto f = integers(readings.value(QString("?FM %1").arg(ch + 1)));
      motorFlags[ch]->setText(
          f.size() == 1 ? Protocol::motorNames(f[0]).join(" · ") : "Estado: —");
    }
    ioValues->setToolTip("Valores brutos das entradas; saídas digitais: " +
                         readings.value("?DO", "—") +
                         "; status M1/M2: " + readings.value("?FM 1", "—") +
                         " / " + readings.value("?FM 2", "—"));
  }
  chart->append(s);
  diagnosticChart->append(s);
  const QVector<double> values{s.amp1, s.amp2, s.voltage,
                               s.rpm1, s.rpm2, s.temperature};
  for (int row = 0; row < values.size(); ++row) {
    const double value = values[row];
    const int decimals = row >= 3 ? 0 : 1;
    if (std::isfinite(value)) {
      minimums[row] =
          std::isfinite(minimums[row]) ? qMin(minimums[row], value) : value;
      maximums[row] =
          std::isfinite(maximums[row]) ? qMax(maximums[row], value) : value;
    }
    for (auto feed : {chartFeed, diagnosticFeed}) {
      feed->item(row, 1)->setText(number(value, decimals));
      feed->item(row, 2)->setText(number(minimums[row], decimals));
      feed->item(row, 3)->setText(number(maximums[row], decimals));
    }
  }
  if (recorded.size() < 36000)
    recorded.append(s);
  else
    notify("Limite de 36.000 amostras atingido. Exporte o CSV para preservar "
           "esta sessão.");
}

void Window::readParameters() {
  if (!session.connected() || applying || readsPending || session.streaming())
    return;
  if (std::any_of(parameters.begin(), parameters.end(),
                  [](const Parameter &p) { return p.dirty; }) &&
      QMessageBox::question(
          this, "Reler parâmetros",
          "Descartar alterações pendentes e reler o controlador?") !=
          QMessageBox::Yes)
    return;
  readsPending = parameters.size();
  nextParameter = 0;
  for (auto &p : parameters) {
    p.known = false;
    p.dirty = false;
  }
  refreshState();
  readNextParameter();
}
void Window::readNextParameter() {
  if (!readsPending) {
    refreshState();
    int known = 0;
    for (const auto &p : parameters)
      if (p.known)
        ++known;
    readProgress->setText(
        QString("%1 de %2 campos lidos").arg(known).arg(parameters.size()));
    notify("Leitura concluída. Campos rejeitados permanecem indisponíveis.");
    return;
  }
  readProgress->setText(
      QString("Lendo %1 / %2").arg(nextParameter + 1).arg(parameters.size()));
  if (nextParameter >= parameters.size() ||
      !session.enqueue("~" + parameters[nextParameter++].suffix)) {
    readsPending = 0;
    refreshState();
    notify("Leitura interrompida.");
  }
}

void Window::updateParameterRow(int row) {
  auto &p = parameters[row];
  p.unit = p.spec->indexUnits.value(QString::number(p.channel)).toString(p.spec->unit);
  const auto mode = [this, &p](const QString &key) -> int {
    for (const auto &other : parameters)
      if (other.key == key && other.channel == p.channel && other.known) {
        // One unit column cannot describe two different capture/control modes.
        if (other.dirty && other.editor &&
            qRound64(other.editor->value() * other.scale) != other.raw)
          return -1;
        return int(other.raw);
      }
    return -1;
  };
  if (p.key == "PCTR" || p.key == "PMIN" || p.key == "PMAX") {
    switch (mode("PMOD")) {
    case 1: p.unit = "µs"; break;
    case 2: p.unit = "Hz"; break;
    case 3: p.unit = "1/4095 ciclo"; break;
    case 6: p.unit = "pulsos"; break;
    default: break; // Disabled, unknown, or a sensor-specific capture mode.
    }
  } else if (p.key == "FEW") {
    switch (mode("MMOD")) {
    case 1: p.unit = "RPM"; break;
    case 3: case 4: p.unit = "contagens"; break;
    default: break;
    }
  } else if ((p.key == "MAC" || p.key == "MDEC" || p.key == "EDEC") &&
             mode("MMOD") == 5) {
    p.unit = "mA/s";
  }
  parameterItems[row]->setText(3, p.unit);
  parameterItems[row]->setToolTip(3, p.unit + (p.spec->unitNote.isEmpty()
                                               ? QString()
                                               : "\n" + p.spec->unitNote));
  if (p.choices) {
    QSignalBlocker block(p.choices);
    p.choices->setCurrentIndex(
        p.choices->findData(qRound(p.editor->value() * p.scale)));
    p.choices->setToolTip(p.choices->currentText());
  }
  parameterItems[row]->setText(
      1, p.known ? QString::number(p.raw / p.scale, 'f',
                                   p.editor ? p.editor->decimals()
                                            : (p.scale == 10 ? 1 : 0))
                 : "—");
}

void Window::applyParameters() {
  if (!session.writesEnabled() || applying || readsPending) {
    notify("Aguarde o término da leitura de parâmetros antes de aplicar.");
    return;
  }
  changes.clear();
  QStringList review;
  for (int row = 0; row < parameters.size(); ++row) {
    const auto &p = parameters[row];
    if (!p.editable || !p.known || !p.dirty)
      continue;
    const qint64 desired = qRound64(p.editor->value() * p.scale);
    if (desired == p.raw)
      continue;
    if (!Catalog::validValue(*p.spec, desired, p.channel)) {
      notify("Valor fora do limite para " + p.suffix);
      return;
    }

    changes.append({row, desired});
    review.append(QString("%1 · canal %2: %3 → %4 %5")
                      .arg(p.name)
                      .arg(p.channel)
                      .arg(p.raw / p.scale)
                      .arg(desired / p.scale)
                      .arg(p.unit));
  }
  if (changes.isEmpty()) {
    notify("Nenhuma alteração pendente.");
    return;
  }
  if (changes.size() > 1 &&
      std::any_of(changes.begin(), changes.end(), [this](const auto &c) {
        return parameters[c.first].spec->disruptive;
      })) {
    notify(
        "Parâmetros de comunicação/segurança devem ser aplicados "
        "individualmente. Aplique ou descarte os outros rascunhos primeiro.");
    return;
  }
  const QString intro =
      session.isDemo() ? "Alterar parâmetros simulados?\n\n"
                       : "Aplicar estes valores na RAM do controlador?\nA "
                         "alteração entra em vigor imediatamente. O script "
                         "será pausado e a parada !EX aplicada.\n\n";
  if (QMessageBox::question(this, "Revisar alterações",
                            intro + review.join('\n') +
                                "\n\nNão será feita gravação na EEPROM.",
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  session.stopStream();
  session.cancelMotion();
  applying = true;
  applyIndex = 0;
  verifyWrite = false;
  operationTarget = "APPLY_RAM";
  operationExpected = "!R 0";
  operationPhase = 0;
  refreshState();
  if (!session.enqueue(operationExpected))
    completeApply(false, "Não foi possível iniciar a alteração.");
}

void Window::applyNext() {
  if (applyIndex >= changes.size()) {
    completeApply(true, "Alterações na RAM confirmadas por releitura.");
    return;
  }
  const auto change = changes[applyIndex];
  const auto &p = parameters[change.first];
  if (!session.enqueue(QString("^%1 %2").arg(p.suffix).arg(change.second)))
    completeApply(
        false, "Alteração interrompida antes de enviar o próximo parâmetro.");
}
void Window::completeApply(bool success, const QString &message) {
  operationExpected.clear();
  operationTarget.clear();
  applying = false;
  verifyWrite = false;
  changes.clear();
  if (!success) {
    session.setWritesEnabled(false);
    for (auto &p : parameters) {
      p.known = false;
      p.dirty = false;
    }
  }
  refreshState();
  notify(message);
  writeLog(success ? "OK" : "ERRO", message);
}

void Window::notify(const QString &message) {
  if (notice) {
    notice->setText(message);
    notice->setToolTip(message);
  }
}
void Window::writeLog(const QString &direction, const QString &message) {

  const QString safe = Protocol::redacted(message);
  const QString log = QDateTime::currentDateTime().toString("HH:mm:ss.zzz") +
                      "  " + direction + "  " + safe;
  console->appendPlainText(log);
  diagnosticLog->appendPlainText(log);
}

bool Window::saveCsv(const QString &path) {
  if (recorded.isEmpty())
    return false;
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;
  QTextStream stream(&f);
  stream << "timestamp_utc,seconds,source,amp1_rms_A,amp2_rms_A,rpm1_encoder,"
            "rpm2_encoder,battery_V,heatsink1_C,encoder1,encoder2,faults_FF,"
            "status_FS,rpm1_hall,rpm2_hall\n";
  auto num = [](double v) {
    return std::isfinite(v) ? QString::number(v, 'f', 3) : QString();
  };
  for (const auto &s : recorded) {
    stream << s.timestamp << ',' << QString::number(s.seconds, 'f', 3) << ','
           << (captureDemo ? "demo" : "serial") << ',' << num(s.amp1) << ','
           << num(s.amp2) << ',' << num(s.rpm1) << ',' << num(s.rpm2) << ','
           << num(s.voltage) << ',' << num(s.temperature) << ','
           << (s.hasEncoder ? QString::number(s.encoder1) : QString()) << ','
           << (s.hasEncoder ? QString::number(s.encoder2) : QString()) << ','
           << (s.hasFaults ? QString::number(s.faults) : QString()) << ','
           << (s.hasFlags ? QString::number(s.flags) : QString()) << ',' << num(s.hallRpm1) << ',' << num(s.hallRpm2) << '\n';
  }
  stream.flush();
  return stream.status() == QTextStream::Ok && f.commit();
}

bool Window::saveSnapshot(const QString &path) { return saveProfile(path); }

void Window::closeEvent(QCloseEvent *event) {
  if (script->document()->isModified() &&
      QMessageBox::question(
          this, "Script não salvo",
          "Fechar e descartar as alterações não salvas do editor?",
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes) {
    event->ignore();
    return;
  }
  session.stopStream();
  session.close();
  event->accept();
}
