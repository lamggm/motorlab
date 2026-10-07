#include "chart.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

Chart::Chart(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setObjectName("telemetryChart");
}
void Chart::append(const Sample &s) {
  history.push_back(s);
  while (history.size() > 1 &&
         (history.front().seconds < s.seconds - 60 || history.size() > 100000))
    history.pop_front();
  update();
}
void Chart::clear() {
  history.clear();
  update();
}
void Chart::setMetric(int index) {
  metric = index;
  update();
}

void Chart::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF rect(57, 18, width() - 78, height() - 59);
  if (rect.width() < 1 || rect.height() < 1)
    return;
  p.fillRect(this->rect(), QColor("#111d30"));
  auto value = [this](const Sample &s, int ch) {
    if (metric == 1)
      return ch ? s.rpm2 : s.rpm1;
    if (metric == 4)
      return ch ? s.hallRpm2 : s.hallRpm1;
    if (metric == 2)
      return s.voltage;
    if (metric == 3)
      return s.temperature;
    return ch ? s.amp2 : s.amp1;
  };
  const double xmax =
      history.empty() ? 60 : std::max(5.0, history.back().seconds);
  const double xmin = std::max(0.0, xmax - 60);
  const double span = xmax - xmin;
  double low = 0, high = (metric == 1 || metric == 4) ? 1000
                         : metric == 2 ? 60
                         : metric == 3 ? 50
                                       : 10;
  for (const auto &s : history)
    if (s.seconds >= xmin) {
      for (int ch = 0; ch < (metric < 2 || metric == 4 ? 2 : 1); ++ch) {
        const double v = value(s, ch);
        if (std::isfinite(v)) {
          low = std::min(low, v);
          high = std::max(high, v * 1.12);
        }
      }
    }
  if (low < 0)
    low *= 1.15;
  if (high <= low)
    high = low + 1;
  p.setFont(QFont("DejaVu Sans", 9));
  for (int i = 0; i <= 4; ++i) {
    const double y = rect.top() + i * rect.height() / 4;
    p.setPen(QColor("#23324a"));
    p.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    p.setPen(QColor("#8396b0"));
    p.drawText(
        QRectF(0, y - 10, 46, 20), Qt::AlignRight | Qt::AlignVCenter,
        QString::number(high - i * (high - low) / 4, 'f', metric == 0 ? 1 : 0));
  }
  for (int i = 0; i <= 4; ++i) {
    const double x = rect.left() + i * rect.width() / 4;
    p.setPen(QColor("#1b2a41"));
    p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    p.setPen(QColor("#8396b0"));
    p.drawText(QRectF(x - 24, rect.bottom() + 8, 48, 20), Qt::AlignCenter,
               QString::number(xmin + i * span / 4, 'f', span < 10 ? 1 : 0) +
                   "s");
  }
  if (history.empty()) {
    p.setPen(QColor("#8396b0"));
    p.drawText(rect, Qt::AlignCenter,
               "Conecte uma porta ou abra a demonstração");
    return;
  }
  p.save();
  p.setClipRect(rect);
  for (int ch = 0; ch < (metric < 2 || metric == 4 ? 2 : 1); ++ch) {
    QPainterPath path;
    bool started = false;
    for (const auto &s : history) {
      if (s.seconds < xmin)
        continue;
      const double v = value(s, ch);
      if (!std::isfinite(v)) {
        started = false;
        continue;
      }
      const QPointF pt(rect.left() + (s.seconds - xmin) / span * rect.width(),
                       rect.bottom() -
                           (v - low) / (high - low) * rect.height());
      if (!started) {
        path.moveTo(pt);
        started = true;
      } else
        path.lineTo(pt);
    }
    p.setPen(QPen(QColor(ch ? "#45d4b5" : "#63a8ff"), 2.5));
    p.drawPath(path);
  }
  p.restore();
}
