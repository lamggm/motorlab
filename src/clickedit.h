#pragma once
#include <QAbstractScrollArea>
#include <QApplication>
#include <QWheelEvent>
#include <utility>

// Scrolling a panel must never edit a parameter or a motor setpoint, even
// when an editor retained keyboard focus from an earlier click. Ignoring
// the editor and forwarding to its scroll area preserves navigation. Combo popups
// keep their own scrolling; choosing an item still requires activation.
template <class Base> class ClickEdit : public Base {
public:
  template <class... Args>
  explicit ClickEdit(Args &&...args) : Base(std::forward<Args>(args)...) {
    this->setFocusPolicy(Qt::StrongFocus);
  }

protected:
  void wheelEvent(QWheelEvent *event) override {
    for (auto parent = this->parentWidget(); parent; parent = parent->parentWidget()) {
      if (auto area = qobject_cast<QAbstractScrollArea *>(parent)) {
        auto viewport = area->viewport();
        QWheelEvent scroll(viewport->mapFromGlobal(event->globalPosition()),
                           event->globalPosition(), event->pixelDelta(),
                           event->angleDelta(), event->buttons(), event->modifiers(),
                           event->phase(), event->inverted(), event->source(),
                           event->pointingDevice());
        QApplication::sendEvent(viewport, &scroll);
        event->setAccepted(scroll.isAccepted());
        return;
      }
    }
    event->ignore();
  }
};
