// -*- Mode: C++ -*-
#ifndef METERWIDGET_H
#define METERWIDGET_H

#include <QWidget>
#include <boost/circular_buffer.hpp>

class MeterWidget : public QWidget
{
  Q_OBJECT
  Q_PROPERTY (int value READ value WRITE setValue)

public:
  explicit MeterWidget (QWidget *parent = 0);

  // value property
  int value () const {return m_signals.back();}
  Q_SLOT void setValue (int value);

  // QWidget implementation
  QSize sizeHint () const override;
  void set_sigPeak(int value);
protected:
  void paintEvent( QPaintEvent * ) override;

private:
  boost::circular_buffer<int> m_signals;
  int                         m_sigPeak;
  int                         m_noisePeak;
};

#endif // METERWIDGET_H
