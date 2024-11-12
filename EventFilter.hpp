#ifndef EVENTFILTER_HPP__
#define EVENTFILTER_HPP__

#include <functional>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>

namespace EventFilter
{
  class Focus final : public QObject
  {
    Q_OBJECT

  public:

    explicit Focus(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool
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

  class EscapeKeyPress final : public QObject
  {
  public:

    using Filter = std::function<bool(QKeyEvent *)>;

    explicit EscapeKeyPress(Filter    filter,
                            QObject * parent = nullptr)
    : QObject {parent}
    , filter_ {filter}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

  private:

    Filter filter_;
  };

  class EnterKeyPress final : public QObject
  {
  public:

    using Filter = std::function<bool(QKeyEvent *)>;

     explicit EnterKeyPress(Filter    filter,
                            QObject * parent = nullptr)
    : QObject {parent}
    , filter_ {filter}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

  private:

    Filter filter_;
  };

  class MouseButtonPress final : public QObject
  {
  public:

    using Filter = std::function<bool(QMouseEvent *)>;

    explicit MouseButtonPress(Filter    filter,
                              QObject * parent = nullptr)
    : QObject {parent}
    , filter_ {filter}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

  private:

    Filter filter_;
  };

  class MouseButtonDblClick final : public QObject
  {
  public:

    using Filter = std::function<bool(QMouseEvent *)>;

    explicit MouseButtonDblClick(Filter    filter,
                                 QObject * parent = nullptr)
    : QObject {parent}
    , filter_ {filter}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

  private:

    Filter filter_;
  };
}


#endif