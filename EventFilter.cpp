#include "EventFilter.hpp"
#include "moc_EventFilter.cpp"

bool
EventFilter::KeyPress::eventFilter(QObject * const object,
                                   QEvent  * const event)
{
  bool processed = false;

  if (event->type() == QEvent::KeyPress)
  {
    Q_EMIT this->keyPressed (object,
                             reinterpret_cast<QKeyEvent *>(event),
                             &processed);
  }

  return processed;
}

bool
EventFilter::EscapeKeyPress::eventFilter(QObject * const object,
                                         QEvent  * const event)
{
  bool processed = false;

  if (event->type() == QEvent::KeyPress)
  {
    if (auto const keyEvent = reinterpret_cast<QKeyEvent *>(event);
                   keyEvent->key() == Qt::Key_Escape)
    {
      Q_EMIT this->escapeKeyPressed (object, keyEvent, &processed);
    }
  }

  return processed;
}

bool
EventFilter::EnterKeyPress::eventFilter(QObject * const object,
                                        QEvent  * const event)
{
  bool processed = false;

  if (event->type() == QEvent::KeyPress)
  {
    if (auto const keyEvent = reinterpret_cast<QKeyEvent *>(event);
                   keyEvent->key() == Qt::Key_Enter ||
                   keyEvent->key() == Qt::Key_Return)
    {
      Q_EMIT this->enterKeyPressed (object, keyEvent, &processed);
    }
  }

  return processed;
}

bool
EventFilter::MouseButtonPress::eventFilter(QObject * const object,
                                           QEvent  * const event)
{
  bool processed = false;

  if (event->type() == QEvent::MouseButtonPress)
  {
    Q_EMIT this->mouseButtonPressed (object,
                                     reinterpret_cast<QMouseEvent *>(event),
                                     &processed);
  }

  return processed;
}

bool
EventFilter::MouseButtonDblClick::eventFilter(QObject * const object,
                                              QEvent  * const event)
{
  bool processed = false;

  if (event->type() == QEvent::MouseButtonDblClick)
  {
    Q_EMIT this->mouseButtonDblClicked (object,
                                        reinterpret_cast<QMouseEvent *>(event),
                                        &processed);
  }

  return processed;
}