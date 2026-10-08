#pragma once
#include <QLabel>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

// Navigation owns category cards; the table contains parameter rows only.
// Paths retain original RoboRun identifiers, independent of translated labels.
class ConfigBrowser : public QWidget {
  Q_OBJECT
public:
  ConfigBrowser(QTreeWidget *table, const QString &id,
                QWidget *parent = nullptr);
  void addParameter(QTreeWidgetItem *item, const QStringList &path);
  void navigate(const QStringList &path);
  int setFilter(const QString &text);
  QStringList currentPath() const { return current; }
signals:
  void clearFilterRequested();

protected:
  bool eventFilter(QObject *object, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  struct Entry {
    QTreeWidgetItem *item;
    QStringList path;
    QString search;
  };
  void rebuild();
  void showHelp(const QString &text);
  QVector<Entry> entries;
  QString id, filter;
  QStringList current;
  QTreeWidget *table;
  QVBoxLayout *layout;
  QWidget *crumbs = nullptr, *cards = nullptr;
  QScrollArea *cardScroll;
  QLabel *summary, *help;
  int columns = 0;
};
