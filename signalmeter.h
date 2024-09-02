// -*- Mode: C++ -*-
#ifndef SIGNALMETER_H
#define SIGNALMETER_H

#include <QFrame>

class QLabel;
class MeterWidget;

class SignalMeter final
  : public QFrame
{
  Q_OBJECT

public:
  explicit SignalMeter (QWidget * parent = nullptr);

public slots:
  void setValue (float value, float valueMax);

private:
  QWidget     * m_scale;
  MeterWidget * m_meter;
  QLabel      * m_reading;
};

#endif // SIGNALMETER_H
