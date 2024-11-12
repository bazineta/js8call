#ifndef EVENTFILTER_HPP__
#define EVENTFILTER_HPP__

#include <QEvent>
#include <QObject>

namespace EventFilter
{
  class Focus : public QObject
  {
    Q_OBJECT

  public:

    explicit Focus(QObject * parent = nullptr)
    : QObject {parent}
    {}

    virtual bool
    eventFilter(QObject * object,
                QEvent  * event) override
    {
      Q_UNUSED(object)
      if      (event->type() == QEvent::FocusIn)  Q_EMIT focused (object);
      else if (event->type() == QEvent::FocusOut) Q_EMIT blurred (object);
      return false;
    }

    Q_SIGNAL void focused(QObject *);
    Q_SIGNAL void blurred(QObject *);
  };
}


#endif