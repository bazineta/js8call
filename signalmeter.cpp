// Simple bargraph dB meter
// Originally implemented by Edson Pereira PY2SDR
//

#include "signalmeter.h"
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPolygon>
#include <QVBoxLayout>
#include <boost/circular_buffer.hpp>
#include "moc_signalmeter.cpp"

class MeterWidget final: public QWidget
{
public:

  static constexpr int MIN = 0;
  static constexpr int MAX = 90;

  explicit MeterWidget (QWidget *parent = nullptr)
  : QWidget  {parent}
  , m_values {10}
  {}

  QSize
  sizeHint() const override
  {
    return {10, 100};
  }

  int
  value() const
  {
    return m_values.back();
  }

  void
  setValue(int value,
           int max)
  {
    m_values.push_back(std::clamp(value, MIN, MAX));
    m_max  = max;
    m_peak = *std::max_element(m_values.begin(),
                               m_values.end());
    update();
  }
 
protected:

  void
  paintEvent(QPaintEvent *) override
  {
    QPainter p {this};
    p.setPen(Qt::NoPen);

    if      (m_max  > 85) { p.setBrush(Qt::red);    }
    else if (m_peak < 15) { p.setBrush(Qt::yellow); }
    else                  { p.setBrush(Qt::green);  }

    auto const target = contentsRect();

    p.drawRect(QRect{QPoint{target.left(),
                            static_cast<int>(target.top() + target.height() - value() / (double)MAX * target.height())},
                    QPoint{target.right(),
                            target.bottom()}});

    if (m_peak)
    {
      p.setRenderHint(QPainter::Antialiasing);
      p.setBrush(Qt::white);
      p.translate(target.left(), static_cast<int>(target.top() + target.height() - m_peak / (double)MAX * target.height()));
      p.drawPolygon(QPolygon { { {target.width(), -4}, {target.width(), 4}, {0, 0} } });
    }
  }

private:

  boost::circular_buffer<int> m_values;
  int                         m_peak;
  int                         m_max;
};

class ScaleWidget final: public QWidget
{
public:

  explicit ScaleWidget (QWidget * parent = nullptr)
    : QWidget {parent}
  {
    setSizePolicy(QSizePolicy::Minimum,
                  QSizePolicy::MinimumExpanding);
  }

  QSize
  sizeHint() const override
  {
    return minimumSizeHint ();
  }

  QSize
  minimumSizeHint() const override
  {
    QFontMetrics font_metrics {font (), nullptr};
    return {tick_length + text_indent + font_metrics.horizontalAdvance("00+"),
            static_cast<int>((font_metrics.height () + line_spacing) * range)};
  }

protected:

  void
  paintEvent (QPaintEvent * event) override
  {
    QWidget::paintEvent (event);

    QPainter p {this};
    p.setPen(Qt::white);
    auto const& target = contentsRect ();
    QFontMetrics font_metrics {p.font (), this};
    auto font_offset = font_metrics.ascent () / 2;
    p.drawLine (target.right (), target.top () + font_offset, target.right (), target.bottom () - font_offset - font_metrics.descent ());
    for (std::size_t i = 0; i <= range; ++i)
      {
        p.save ();
        p.translate (target.right() - tick_length,
                     target.top () + font_offset + i * (target.height () - font_metrics.ascent () - font_metrics.descent ()) / range);
        p.drawLine (0, 0, tick_length, 0);
        if (i & 1) {
          auto text = QString::number ((range - i) * scale);
          p.drawText (-(text_indent + font_metrics.horizontalAdvance(text)), font_offset, text);
        }
        p.restore ();
      }
  }

private:

  static constexpr int         tick_length  {4};
  static constexpr int         text_indent  {2};
  static constexpr int         line_spacing {0};
  static constexpr std::size_t scale        {10};
  static constexpr std::size_t range        {MeterWidget::MAX / scale};
};

SignalMeter::SignalMeter (QWidget * parent)
  : QFrame  {parent}
  , m_scale {new ScaleWidget}
  , m_meter {new MeterWidget}
{
  auto outer_layout = new QVBoxLayout;
  outer_layout->setSpacing (0);

  auto inner_layout = new QHBoxLayout;
  inner_layout->setContentsMargins (9, 0, 9, 0);
  inner_layout->setSpacing (0);

  m_meter->setSizePolicy(QSizePolicy::Minimum,
                         QSizePolicy::Minimum);

  inner_layout->addWidget (m_scale);
  inner_layout->addWidget (m_meter);

  m_reading = new QLabel(this);
  m_reading->setAlignment(Qt::AlignRight);
  m_reading->setContentsMargins(9, 5, 9, 0);
  m_reading->setStyleSheet("QLabel { color: white }");

  outer_layout->addLayout (inner_layout);
  outer_layout->addWidget (m_reading);
  setLayout (outer_layout);
}

void
SignalMeter::setValue(float value,
                      float valueMax)
{
  if(value<0) value=0;
  QFontMetrics font_metrics {m_scale->font (), nullptr};
  m_meter->setContentsMargins (0, font_metrics.ascent () / 2, 0, font_metrics.ascent () / 2 + font_metrics.descent ());
  m_meter->setValue(value, valueMax);
  m_reading->setText(QString("%1 dB").arg(int(value+0.5)));
}
