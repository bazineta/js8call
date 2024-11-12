#ifndef FOCUSEATER_HPP__
#define FOCUSEATER_HPP__

#include <QEvent>
#include <QObject>

class FocusEater : public QObject
{
   Q_OBJECT

public:

  explicit FocusEater(QObject * parent = nullptr)
  : QObject {parent}
  {}

  virtual bool
  eventFilter(QObject * object,
              QEvent  * event) override
  {
    Q_UNUSED(object)
    if      (event->type() == QEvent::FocusIn)  emit focused(object);
    else if (event->type() == QEvent::FocusOut) emit blurred(object);
    return false;
  }

  Q_SIGNAL void focused(QObject *);
  Q_SIGNAL void blurred(QObject *);
};

#endif