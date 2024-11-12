#ifndef EVENTFILTER_HPP__
#define EVENTFILTER_HPP__

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

  class KeyPress final : public QObject
  {
    Q_OBJECT

  public:

    explicit KeyPress(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

    Q_SIGNAL void keyPressed (QObject   *,
                              QKeyEvent *,
                              bool      *);
  };

  class EscapeKeyPress final : public QObject
  {
    Q_OBJECT

  public:

    explicit EscapeKeyPress(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

    Q_SIGNAL void escapeKeyPressed (QObject   *,
                                    QKeyEvent *,
                                    bool      *);
  };

  class EnterKeyPress final : public QObject
  {
    Q_OBJECT

  public:

    explicit EnterKeyPress(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

    Q_SIGNAL void enterKeyPressed (QObject   *,
                                   QKeyEvent *,
                                   bool      *);
  };

  class MouseButtonPress final : public QObject
  {
    Q_OBJECT

  public:

    explicit MouseButtonPress(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

    Q_SIGNAL void mouseButtonPressed (QObject     *,
                                      QMouseEvent *,
                                      bool        *);
  };

  class MouseButtonDblClick final : public QObject
  {
    Q_OBJECT

  public:

    explicit MouseButtonDblClick(QObject * parent = nullptr)
    : QObject {parent}
    {}

    bool eventFilter(QObject *,
                     QEvent  *) override;

    Q_SIGNAL void mouseButtonDblClicked (QObject     *,
                                         QMouseEvent *,
                                         bool        *);
  };
}


#endif