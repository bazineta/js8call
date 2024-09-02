// Simple bargraph meter
// Originally implemented by Edson Pereira PY2SDR

#include "meterwidget.h"
#include <QPainter>
#include <QPolygon>
#include "moc_meterwidget.cpp"

MeterWidget::MeterWidget(QWidget * parent)
  : QWidget     {parent}
  , m_signals   {10}
  , m_sigPeak   {0}
  , m_noisePeak {0}
{
}

QSize
MeterWidget::sizeHint() const
{
  return {10, 100};
}

void
MeterWidget::setValue(int value)
{
  m_signals.push_back(std::clamp(value, MIN, MAX));
  m_noisePeak = *std::max_element(m_signals.begin(),
                                  m_signals.end());
  update();
}

void
MeterWidget::set_sigPeak(int value)
{
  m_sigPeak = value;
}

void
MeterWidget::paintEvent(QPaintEvent *)
{
  QPainter p {this};
  p.setPen(Qt::NoPen);

  if      (m_sigPeak   > 85) { p.setBrush(Qt::red);    }
  else if (m_noisePeak < 15) { p.setBrush(Qt::yellow); }
  else                       { p.setBrush(Qt::green);  }

  auto const target = contentsRect();

  p.drawRect(QRect{QPoint{target.left(),
                          static_cast<int>(target.top() + target.height() - value() / (double)MAX * target.height())},
                   QPoint{target.right(),
                          target.bottom()}});

  if (m_noisePeak)
  {
    // Draw peak hold indicator
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::white);
    p.translate(target.left(), static_cast<int>(target.top() + target.height() - m_noisePeak / (double)MAX * target.height()));
    p.drawPolygon(QPolygon { { {target.width(), -4}, {target.width(), 4}, {0, 0} } });
  }
}
