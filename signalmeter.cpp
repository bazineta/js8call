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


class SignalMeter::Meter final: public QWidget
{
public:

  static constexpr int MIN = 0;
  static constexpr int MAX = 90;

  explicit Meter(QWidget * parent)
  : QWidget  {parent}
  , m_values {10}
  {}

  QSize
  sizeHint() const override
  {
    return {10, 100};
  }

  int last() const { return m_values.back(); }
  int peak() const { return m_peak;          }
  int max()  const { return m_max;           }

  void
  setValue(const int value,
           const int valueMax)
  {
    auto const oldLast = last();
    auto const oldPeak = peak();
    auto const oldMax  = max();

    m_values.push_back(std::clamp(value, MIN, MAX));
    m_peak = *std::max_element(m_values.begin(),
                               m_values.end());
    m_max  = valueMax;

    if (last() != oldLast ||
        peak() != oldPeak ||
        max()  != oldMax) update();
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
                            static_cast<int>(target.top() + target.height() - last() / (double)MAX * target.height())},
                     target.bottomRight()});

    if (peak())
    {
      p.setRenderHint(QPainter::Antialiasing);
      p.setBrush(Qt::white);
      p.translate(target.left(), static_cast<int>(target.top() + target.height() - peak() / (double)MAX * target.height()));
      p.drawPolygon(QPolygon { { {target.width(), -4}, {target.width(), 4}, {0, 0} } });
    }
  }

private:

  boost::circular_buffer<int> m_values;
  int                         m_peak;
  int                         m_max;
};

class SignalMeter::Scale final: public QWidget
{
private:

  static constexpr int         tick_length  {4};
  static constexpr int         text_indent  {2};
  static constexpr int         line_spacing {0};
  static constexpr std::size_t scale        {10};
  static constexpr std::size_t range        {Meter::MAX / scale};

public:

  explicit Scale(QWidget * parent)
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
    QFontMetrics metrics{font(), this};
    return {tick_length + text_indent + metrics.horizontalAdvance("00+"),
            static_cast<int>((metrics.height () + line_spacing) * range)};
  }

protected:

  void
  paintEvent (QPaintEvent * event) override
  {
    QWidget::paintEvent (event);

    auto const target  = contentsRect();
    auto const metrics = QFontMetrics(font(), this);
    auto const margin  = metrics.height() / 2;
    auto const offset  = metrics.height() / 4;

    QPainter p {this};
    p.setPen(Qt::white);

    p.drawLine(target.right(), target.top()    + margin,
               target.right(), target.bottom() - margin);

    for (std::size_t i = 0; i <= range; ++i)
    {
      p.save();
      p.translate(target.right() - tick_length,
                  target.top()   + margin + i * (target.height() - metrics.height()) / range);
      p.drawLine(0, 0, tick_length, 0);
      if (i & 1) {
        auto const text = QString::number((range - i) * scale);
        p.drawText(-(text_indent + metrics.horizontalAdvance(text)), offset, text);
      }
      p.restore ();
    }
  }
};

SignalMeter::SignalMeter(QWidget * parent)
  : QFrame  {parent}
  , m_scale {new Scale {this}}
  , m_meter {new Meter {this}}
  , m_value {new QLabel{this}}
{
  auto outer_layout = new QVBoxLayout;
  outer_layout->setSpacing(8);

  auto inner_layout = new QHBoxLayout;
  inner_layout->setContentsMargins(9, 0, 9, 0);
  inner_layout->setSpacing(0);

  auto label_layout = new QHBoxLayout;
  label_layout->setSpacing(4);

  auto const margin = QFontMetrics(m_scale->font(),
                                   m_scale).height() / 2;

  m_meter->setContentsMargins(0, margin, 0, margin);
  m_meter->setSizePolicy(QSizePolicy::Minimum,
                         QSizePolicy::Minimum);

  m_value->setAlignment(Qt::AlignRight);

  inner_layout->addWidget(m_scale);
  inner_layout->addWidget(m_meter);

  label_layout->addWidget(m_value);
  label_layout->addWidget(new QLabel("dB", this));

  outer_layout->addLayout(inner_layout);
  outer_layout->addLayout(label_layout);

  setLayout (outer_layout);
}

void
SignalMeter::setValue(const float value,
                      const float valueMax)
{
  m_meter->setValue(value, valueMax);
  m_value->setText(QString::number(int(value + 0.5)));
}
