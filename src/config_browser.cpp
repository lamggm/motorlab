#include "config_browser.h"
#include "config_text.h"
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QTextDocument>
#include <cmath>

namespace {
QString searchable(const QString &text) {
  QString result;
  for (auto ch : text.normalized(QString::NormalizationForm_D).toLower())
    if (ch.category() != QChar::Mark_NonSpacing)
      result += ch;
  return result;
}
QPixmap illustration(const QString &group) {
  QPixmap image(200, 140);
  image.fill(Qt::transparent);
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor blue("#63a8ff"), mint("#45d4b5");
  p.setPen(QPen(blue, 3));
  p.setBrush(QColor("#142a43"));
  if (group.startsWith("Motor ") && group != "Motor Output" &&
      group != "Motor Parameters" && group != "Motor Thermistor") {
    // Finned BLDC body, mounting flange, shaft and cable connector.
    p.drawRoundedRect(QRectF(39, 32, 98, 74), 10, 10);
    for (int x = 53; x < 128; x += 13)
      p.drawLine(x, 40, x, 99);
    p.setBrush(QColor("#203f5f"));
    p.drawRoundedRect(QRectF(133, 27, 18, 84), 5, 5);
    p.setBrush(mint);
    p.drawRoundedRect(QRectF(151, 59, 31, 20), 3, 3);
    p.setBrush(QColor("#142a43"));
    p.drawRoundedRect(QRectF(66, 13, 33, 19), 4, 4);
    p.setPen(QPen(mint, 3));
    p.drawLine(77, 13, 77, 5);
    p.drawLine(89, 13, 89, 5);
    p.setPen(QPen(blue, 3));
    p.drawLine(55, 108, 48, 124);
    p.drawLine(121, 108, 128, 124);
    p.drawLine(45, 125, 132, 125);
  } else if (group == "Motor Output" || group.contains("Power")) {
    p.drawRoundedRect(QRectF(43, 26, 116, 87), 12, 12);
    QPolygonF bolt{QPointF(110, 36), QPointF(75, 75),  QPointF(99, 75),
                   QPointF(88, 102), QPointF(126, 62), QPointF(103, 62)};
    p.setPen(Qt::NoPen);
    p.setBrush(mint);
    p.drawPolygon(bolt);
  } else if (group.contains("Speed") || group.contains("Loop") ||
             group.contains("Trajectory")) {
    p.drawArc(QRectF(43, 22, 113, 108), 0, 180 * 16);
    p.setPen(QPen(mint, 5));
    p.drawLine(100, 78, 134, 44);
    p.drawEllipse(QPointF(100, 78), 5, 5);
    p.setPen(QPen(blue, 3));
    p.drawLine(42, 115, 162, 115);
    p.setPen(QPen(mint, 3));
    p.drawPolyline(QPolygonF{QPointF(45, 111), QPointF(65, 111),
                             QPointF(108, 94), QPointF(156, 94)});
  } else if (group.contains("Safety") || group.contains("Limits") ||
             group.contains("Guard") || group == "General" ||
             group.contains("I2T")) {
    QPolygonF shield{QPointF(100, 18),  QPointF(151, 37), QPointF(143, 86),
                     QPointF(100, 125), QPointF(57, 86),  QPointF(49, 37)};
    p.drawPolygon(shield);
    p.setPen(QPen(mint, 7));
    p.drawPolyline(
        QPolygonF{QPointF(74, 70), QPointF(93, 89), QPointF(128, 53)});
  } else if (group.contains("Encoder") || group.contains("Sensor") ||
             group.contains("Sin") || group.contains("Hall") ||
             group.contains("Direction")) {
    p.drawEllipse(QPointF(100, 71), 49, 49);
    p.drawEllipse(QPointF(100, 71), 17, 17);
    for (int i = 0; i < 12; ++i) {
      p.save();
      p.translate(100, 71);
      p.rotate(i * 30);
      p.drawLine(0, -39, 0, -47);
      p.restore();
    }
    p.setPen(QPen(mint, 4));
    p.drawPolyline(
        QPolygonF{QPointF(40, 125), QPointF(40, 112), QPointF(70, 112),
                  QPointF(70, 125), QPointF(99, 125), QPointF(99, 112),
                  QPointF(129, 112), QPointF(129, 125), QPointF(159, 125)});
  } else if (group.contains("Therm") || group.contains("Mechanical")) {
    p.drawRoundedRect(QRectF(85, 20, 30, 80), 14, 14);
    p.drawEllipse(QPointF(100, 106), 22, 22);
    p.setPen(QPen(mint, 8));
    p.drawLine(100, 47, 100, 105);
  } else if (group.contains("Input") || group.contains("Output") ||
             group.contains("In") || group.contains("Out")) {
    p.drawRoundedRect(QRectF(43, 33, 116, 75), 10, 10);
    p.setPen(QPen(mint, 4));
    p.drawPolyline(QPolygonF{QPointF(29, 74), QPointF(64, 74), QPointF(64, 51),
                             QPointF(100, 51), QPointF(100, 89),
                             QPointF(136, 89), QPointF(136, 66),
                             QPointF(174, 66)});
  } else if (group == "CAN" || group.contains("PDO") || group.contains("COB") ||
             group.contains("Heartbeat") || group.contains("RS232") ||
             group == "ModBus") {
    p.drawLine(38, 76, 163, 76);
    for (int x : {51, 100, 149}) {
      p.drawLine(x, 76, x, 40);
      p.drawRoundedRect(QRectF(x - 15, 20, 30, 23), 5, 5);
    }
    p.setPen(QPen(mint, 4));
    p.drawLine(100, 78, 100, 109);
    p.drawRoundedRect(QRectF(85, 110, 30, 19), 4, 4);
  } else if (group.contains("Start") || group == "Scripting" ||
             group == "Commands") {
    p.drawRoundedRect(QRectF(45, 22, 110, 99), 12, 12);
    p.setPen(QPen(mint, 5));
    p.drawPolyline(
        QPolygonF{QPointF(73, 52), QPointF(92, 70), QPointF(73, 88)});
    p.drawLine(105, 88, 127, 88);
  } else {
    p.drawRoundedRect(QRectF(46, 24, 109, 98), 12, 12);
    for (int y : {48, 73, 98}) {
      p.drawLine(63, y, 139, y);
      p.setBrush(mint);
      p.drawEllipse(QPointF(y == 73 ? 116 : 85, y), 6, 6);
    }
  }
  return image;
}
class ConfigCard : public QPushButton {
public:
  ConfigCard(const QString &original, int count, QWidget *parent)
      : QPushButton(parent), picture(illustration(original)),
        title(ConfigText::category(original)), count(count) {
    setText(title);
    setAccessibleName(title);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(190, 168);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(ConfigText::categoryTooltip(original));
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(underMouse() ? QColor("#1b3553") : QColor("#14273e"));
    p.setPen(QPen(hasFocus()     ? QColor("#45d4b5")
                  : underMouse() ? QColor("#63a8ff")
                                 : QColor("#355678"),
                  hasFocus() ? 2 : 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
    p.drawPixmap(QRect(width() / 2 - 56, 7, 112, 78), picture);
    p.setPen(QColor("#edf5ff"));
    auto f = font();
    f.setPixelSize(15);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(10, 87, width() - 20, 38),
               Qt::AlignCenter | Qt::TextWordWrap, title);
    f.setPixelSize(11);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#adbed2"));
    p.drawText(QRect(12, 127, width() - 24, 20), Qt::AlignCenter,
               QString("%1 parâmetros").arg(count));
  }

private:
  QPixmap picture;
  QString title;
  int count;
};
} // namespace
ConfigBrowser::ConfigBrowser(QTreeWidget *table, const QString &id,
                             QWidget *parent)
    : QWidget(parent), id(id), table(table) {
  setObjectName(id + "Browser");
  layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(9);
  summary = new QLabel;
  summary->setObjectName(id + "Summary");
  summary->setWordWrap(true);
  cardScroll = new QScrollArea;
  cardScroll->setWidgetResizable(true);
  cardScroll->setFrameShape(QFrame::NoFrame);
  cardScroll->setStyleSheet("QScrollArea { background:#111d30; } "
                            "QWidget#configCards { background:#111d30; }");
  cardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout->addWidget(summary);
  layout->addWidget(cardScroll, 1);
  layout->addWidget(table, 1);
  help = new QLabel("Passe o mouse sobre uma opção para ver o nome original e "
                    "sua descrição.");
  help->setObjectName(id + "ParameterHelp");
  help->setWordWrap(true);
  help->setMinimumHeight(70);
  help->setStyleSheet(
      "QLabel { background:#102137; border:1px solid #29445f; "
      "border-radius:8px; padding:10px; color:#adbed2; font-size:12px; }");
  layout->addWidget(help);
  table->setRootIsDecorated(false);
  table->setIndentation(0);
  table->setMouseTracking(true);
  connect(table, &QTreeWidget::itemEntered, this,
          [this](QTreeWidgetItem *item, int) { showHelp(item->toolTip(0)); });
  connect(table, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *item) {
            if (item)
              showHelp(item->toolTip(0));
          });
}
void ConfigBrowser::addParameter(QTreeWidgetItem *item,
                                 const QStringList &path) {
  QStringList translated;
  for (const auto &part : path)
    translated.append(ConfigText::category(part));
  item->setData(0, Qt::UserRole + 1, path);
  QTextDocument text;
  text.setHtml(item->toolTip(0));
  entries.append({item, path,
                  searchable(translated.join(" ") + " " +
                             path.join(" ").replace("&amp;", "&") + " " +
                             item->text(0) + " " + text.toPlainText())});
  if (auto editor = table->itemWidget(item, 2)) {
    editor->setToolTip(item->toolTip(0));
    editor->installEventFilter(this);
  }
}
void ConfigBrowser::navigate(const QStringList &path) {
  current = path;
  rebuild();
}
int ConfigBrowser::setFilter(const QString &text) {
  filter = searchable(text.trimmed());
  rebuild();
  int count = 0;
  for (const auto &e : entries)
    if (!e.item->isHidden())
      ++count;
  return count;
}
void ConfigBrowser::showHelp(const QString &text) {
  // Keep the explanation readable below the table; protocol references remain
  // in the full hover tooltip.
  help->setText(text.split("<br/><br/>").mid(0, 2).join("<br/><br/>"));
}
bool ConfigBrowser::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::Enter)
    if (auto widget = qobject_cast<QWidget *>(object))
      showHelp(widget->toolTip());
  return QWidget::eventFilter(object, event);
}
void ConfigBrowser::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (columns != qBound(1, (width() - 20) / 215, 4))
    rebuild();
}
void ConfigBrowser::rebuild() {
  columns = qBound(1, (width() - 20) / 215, 4);
  if (crumbs) {
    layout->removeWidget(crumbs);
    crumbs->hide();
    crumbs->deleteLater();
  }
  crumbs = new QWidget;
  auto trail = new QHBoxLayout(crumbs);
  trail->setContentsMargins(0, 0, 0, 0);
  auto home = new QPushButton("⌂  Início");
  home->setObjectName(id + "Home");
  // Old navigation objects must not shadow the newly created controls in
  // automation.
  for (auto old : findChildren<QPushButton *>())
    if (old != home && old->parent() != crumbs)
      old->setObjectName("");
  trail->addWidget(home);
  connect(home, &QPushButton::clicked, this, [this] {
    if (!filter.isEmpty())
      emit clearFilterRequested();
    navigate({});
  });
  if (filter.isEmpty() && !current.isEmpty()) {
    auto back = new QPushButton("← Voltar");
    back->setObjectName(id + "Back");
    trail->addWidget(back);
    connect(back, &QPushButton::clicked, this,
            [this] { navigate(current.mid(0, current.size() - 1)); });
    trail->addWidget(new QLabel("›"));
    auto title = new QLabel(ConfigText::category(current.last()));
    title->setWordWrap(true);
    title->setStyleSheet("font-size:15px; font-weight:600;");
    QStringList path;
    for (const auto &part : current)
      path.append(ConfigText::category(part));
    title->setToolTip(path.join(" / "));
    trail->addWidget(title, 1);
  } else if (!filter.isEmpty())
    trail->addWidget(new QLabel("Resultados da busca em todos os grupos"));
  trail->addStretch();
  layout->insertWidget(0, crumbs);
  if (auto old = cardScroll->takeWidget()) {
    old->hide();
    old->deleteLater();
  }
  cards = new QWidget;
  cards->setObjectName("configCards");
  cards->setAttribute(Qt::WA_StyledBackground, true);
  auto grid = new QGridLayout(cards);
  grid->setContentsMargins(1, 1, 8, 1);
  grid->setSpacing(12);
  QMap<QString, int> groups;
  int shown = 0;
  for (const auto &e : entries) {
    bool inside = e.path.mid(0, current.size()) == current;
    bool visible = filter.isEmpty() ? inside && e.path.size() == current.size()
                                    : e.search.contains(filter);
    e.item->setHidden(!visible);
    if (visible)
      ++shown;
    if (filter.isEmpty() && inside && e.path.size() > current.size())
      ++groups[e.path[current.size()]];
  }
  int cell = 0;
  // Motors are the first cards in the power section.
  QStringList order = groups.keys();
  for (const auto &motor : QStringList{"Motor 2", "Motor 1"})
    if (order.removeOne(motor))
      order.prepend(motor);
  for (const auto &key : order) {
    auto b = new ConfigCard(key, groups[key], cards);
    auto path = current;
    path.append(key);
    b->setObjectName(id + "Card_" + path.join("|"));
    b->setProperty("configPath", path);
    connect(b, &QPushButton::clicked, this, [this, path] { navigate(path); });
    b->installEventFilter(this);
    grid->addWidget(b, cell / columns, cell % columns);
    ++cell;
  }
  grid->setRowStretch((cell + columns - 1) / columns, 1);
  cardScroll->setWidget(cards);
  cardScroll->setVisible(!groups.isEmpty());
  cardScroll->setMaximumHeight(shown ? 195 : QWIDGETSIZE_MAX);
  table->setVisible(shown > 0);
  help->setVisible(shown > 0 || !groups.isEmpty());
  summary->setText(!filter.isEmpty()
                       ? QString("%1 parâmetros encontrados · a busca inclui "
                                 "nomes em português, originais e comandos")
                             .arg(shown)
                   : groups.isEmpty()
                       ? QString("%1 parâmetros · edite os rascunhos e aplique "
                                 "nas ações ao lado")
                             .arg(shown)
                       : QString("Selecione um grupo · %1 categorias%2")
                             .arg(groups.size())
                             .arg(shown ? QString(" e %1 parâmetro%2")
                                              .arg(shown)
                                              .arg(shown == 1 ? QString() : "s")
                                        : QString()));
  auto selected = table->currentItem();
  if (selected && !selected->isHidden())
    showHelp(selected->toolTip(0));
  else
    help->setText("Passe o mouse sobre uma opção para ver o nome original e "
                  "sua descrição.");
  table->scrollToTop();
}
