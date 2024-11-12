#include "EventFilter.hpp"
#include "moc_EventFilter.cpp"

bool
EventFilter::EscapeKeyPress::eventFilter(QObject * const,
                                         QEvent  * const event)
{
  if (event->type() == QEvent::KeyPress)
  {
    if (auto const keyEvent = reinterpret_cast<QKeyEvent *>(event);
                   keyEvent->key() == Qt::Key_Escape)
      {
        return filter_(keyEvent);
      }
  }

  return false;
}

bool
EventFilter::EnterKeyPress::eventFilter(QObject * const,
                                        QEvent  * const event)
{
  if (event->type() == QEvent::KeyPress)
  {
    if (auto const keyEvent = reinterpret_cast<QKeyEvent *>(event);
                   keyEvent->key() == Qt::Key_Enter ||
                   keyEvent->key() == Qt::Key_Return)
    {
      return filter_(keyEvent);
    }
  }

  return false;
}

bool
EventFilter::MouseButtonPress::eventFilter(QObject * const,
                                           QEvent  * const event)
{
  if (event->type() == QEvent::MouseButtonPress)
  {
    return filter_(reinterpret_cast<QMouseEvent *>(event));
  }

  return false;
}

bool
EventFilter::MouseButtonDblClick::eventFilter(QObject *,
                                              QEvent  * const event)
{
  if (event->type() == QEvent::MouseButtonDblClick)
  {
    return filter_(reinterpret_cast<QMouseEvent *>(event));
  }

  return false;
}