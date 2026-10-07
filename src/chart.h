#pragma once
#include <deque>
#include <QWidget>
#include <limits>

struct Sample {
  double seconds = 0;
  QString timestamp;
  double amp1 = qQNaN(), amp2 = qQNaN(), rpm1 = qQNaN(), rpm2 = qQNaN();
  double hallRpm1 = qQNaN(), hallRpm2 = qQNaN();
  double voltage = qQNaN(), temperature = qQNaN();
  qint64 encoder1 = 0, encoder2 = 0, faults = 0, flags = 0;
  bool hasEncoder = false, hasFaults = false, hasFlags = false;
};

class Chart : public QWidget {
public:
  explicit Chart(QWidget *parent = nullptr);
  void append(const Sample &sample);
  void clear();
  void setMetric(int index);
  const std::deque<Sample> &data() const { return history; }

protected:
  void paintEvent(QPaintEvent *) override;

private:
  std::deque<Sample> history;
  int metric = 0;
};
