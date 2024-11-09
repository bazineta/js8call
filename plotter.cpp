#include "plotter.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QScopedValueRollback>
#include <QToolTip>
#include <QWheelEvent>
#include "commons.h"
#include "moc_plotter.cpp"
#include "DriftingDateTime.h"
#include "JS8Submode.hpp"

extern "C" {
  void flat4_(float swide[], int* iz, bool* bflatten);
  void plotsave_(float swide[], int* m_w , int* m_h1, int* irow);
}

namespace
{
  // 30 meter band: 10.130-10.140 RTTY
  //                10.140-10.150 Packet

  constexpr float BAND_30M_START = 10.13f;
  constexpr float BAND_30M_END   = 10.15f;
  
  // The WSPR range starts at 10.1401 MHz and runs for 200 Hz.

  constexpr float WSPR_START = 10.1401f;
  constexpr int   WSPR_RANGE = 200;

  // FFT bin width, as with NSPS, a constant; see the JT9 documentation
  // for the reasoning behind the values used here, but in short, since
  // NSPS is always 6912, 1500 for nsps2 and 2048 for nfft3 are optimal.

  constexpr double FFT_BIN_WIDTH = 1500.0 / 2048.0;

  // Vertical divisions in the spectrum display.

  constexpr std::size_t VERT_DIVS = 7;

  // Given a floating point value, return the fractional portion of the
  // value e.g., 42.7 -> 0.7.

  template <typename T,
            typename = std::enable_if_t<std::is_floating_point_v<T>>>
  constexpr auto
  fractionalPart(T const v)
  {
    T                    integralPart;
    return std::modf(v, &integralPart);
  }

  // Given the frequency span of the entire viewable plot region, return
  // the frequency span that each division should occupy.

  int
  freqPerDiv(double const fSpan)
  {
    if (fSpan > 2500) { return 500; }
    if (fSpan > 1000) { return 200; }
    if (fSpan >  500) { return 100; }
    if (fSpan >  250) { return  50; }
    if (fSpan >  100) { return  20; }
                        return  10;
  }
}

// Our paint event is going to completely paint over our entire areaa with
// opaque content, i.e., it's going to blit 3 pixmaps for scale, waterfall,
// and spectrum, all of which begin life being filled with an opaque color.
// We therefore set the Qt::WA_OpaquePaintEvent, attribute, avoiding any
// unnecessary overhead associated with repainting the background.

CPlotter::CPlotter(QWidget * parent)
  : QWidget        {parent}
  , m_freqPerPixel {m_binsPerPixel * FFT_BIN_WIDTH}
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
}

CPlotter::~CPlotter() = default;

QSize
CPlotter::minimumSizeHint() const
{
  return QSize(50, 50);
}

QSize
CPlotter::sizeHint() const
{
  return QSize(180, 180);
}

void
CPlotter::resizeEvent(QResizeEvent *)
{
  if (!size().isValid()) return;

  auto const makePixmap = [dpr = devicePixelRatio()](QSize  const & size,
                                                     QColor const & fill)
  {
    auto pixmap = QPixmap(size * dpr);
    
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(fill);

    return pixmap;
  };

  if ((m_size            != size()) ||
      (m_percent2DScreen != m_percent2DScreen0))
  {
    m_size = size();
    m_w    = m_size.width();
    m_h    = m_size.height();
    m_h2   = m_percent2DScreen * m_h / 100.0;
    
    if (m_h2 > m_h - 30) m_h2 = m_h - 30;
    if (m_h2 <        1) m_h2 =        1;
    
    m_h1 = m_h - m_h2;

    m_ScalePixmap     = makePixmap({m_w,   30}, Qt::white);
    m_WaterfallPixmap = makePixmap({m_w, m_h1}, Qt::black);
    m_OverlayPixmap   = makePixmap({m_w, m_h2}, Qt::black);

    drawDials();
    drawFilter();

    // The overlay pixmap acts as a prototype for the spectrum pixmap;
    // each time we draw the spectrum, we do so by first making a copy
    // of the overlay, then drawing the spectrum line into it.

    m_SpectrumPixmap   = m_OverlayPixmap.copy();
    m_percent2DScreen0 = m_percent2DScreen;
  }
  drawOverlay();
}

void
CPlotter::paintEvent(QPaintEvent *)
{
  QPainter p(this);

  p.drawPixmap(0,    0, m_ScalePixmap);
  p.drawPixmap(0,   30, m_WaterfallPixmap);
  p.drawPixmap(0, m_h1, m_SpectrumPixmap);

  auto const x = xFromFreq(freq());

  p.drawPixmap(x, 30, m_DialPixmap[0]);

  if (m_lastMouseX >= 0 &&
      m_lastMouseX != x)
  {
    p.drawPixmap(m_lastMouseX, 30, m_DialPixmap[1]);
  }

  if (m_filterEnabled && m_filterWidth > 0)
  {
    p.drawPixmap(                                                      0, 0, m_FilterPixmap[0]);
    p.drawPixmap(m_w - m_FilterPixmap[1].deviceIndependentSize().width(), 0, m_FilterPixmap[1]);
  }
}

void
CPlotter::draw(float      swide[],
               bool const bScroll)
{
  // Move current data down one line; we must do this before
  // attaching a QPainter.

  if (bScroll && !m_replot)
  {
    m_WaterfallPixmap.scroll(0, 1, m_WaterfallPixmap.rect());
  }

  QPainter p(&m_WaterfallPixmap);

  auto iz = xFromFreq(5000.0);

  if (bScroll && swide[0] < 1.e29)
  {
    flat4_(swide, &iz, &m_flatten);
  }

  if(swide[0] > 1.e29 && swide[0] < 1.5e30) p.setPen(Qt::green); // horizontal line
  if(swide[0] > 1.4e30                    ) p.setPen(Qt::yellow);

  if (!m_replot)
  {
    m_j      =  0;
    int irow = -1;

    plotsave_(swide, &m_w, &m_h1, &irow);
  }

  double const fac    = sqrt(m_binsPerPixel * m_waterfallAvg / 15.0);
  double const gain   = fac * pow(10.0, 0.015 * m_plotGain);
  double const gain2d =       pow(10.0, 0.02  * m_plot2dGain);
  auto   const base   = static_cast<int>(m_startFreq / FFT_BIN_WIDTH + 0.5);
  auto         ymin   = 1.e30f;

  // First loop; draws points into the waterfall and determines the
  // minimum y extent.

  for(int i = 0; i < iz; i++)
  {
    float const y = swide[i];
    if (y < ymin ) ymin = y;
    if (y < 1.e29) p.setPen(m_colors[std::clamp(static_cast<int>(10.0 * gain * y + m_plotZero), 0, 254)]);
    p.drawPoint(i, m_j);
  }

  // Summarization method, used when scrolling and during computation of
  // linear average.

  auto const sum = [base,
                    bins = m_binsPerPixel](float const * const data,
                                           auto  const         index)
  {
    float sum = 0.0f;
    int   k   = base + bins * index;

    for (int l = 0; l < bins; l++)
    {
      sum += data[k++];
    }
    
    return sum;
  };

  // Second loop, determines how we're going to draw the spectrum.
  // Updates the sums if we're scrolling, updates the points to draw.

  for (int i = 0; i < iz; i++)
  {
    if (bScroll)
    {
      m_sum[i] = sum(dec_data.savg, i);
    }

    float y = 0;

    switch (m_spectrum)
    {
      case Spectrum::Current:
        y = gain2d * (swide[i] - ymin) + m_plot2dZero  + (m_flatten ? 0 : 15);
      break;
      case Spectrum::Cumulative:
        y = gain2d * (m_sum[i] / m_binsPerPixel + m_plot2dZero) + (m_flatten ? 0 : 15);
      break;
      case Spectrum::LinearAvg:
        y = 2.0 * gain2d * sum(spectra_.syellow, i) / m_binsPerPixel + m_plot2dZero;
      break;
    }

    m_points[i].setX(i);
    m_points[i].setY(int(0.9 * m_h2 - y * m_h2 / 70.0));
  }

  drawSpectrum(iz - 1);

  if (m_replot) return;

  // If we've just drawn a decode line, compute the number of lines required
  // before we need to draw the decode text. If that wasn't a decode line,
  // see if we've reached the point where we should draw the decode text.

  if (swide[0] > 1.0e29)
  {
    m_line = p.fontMetrics().height() * devicePixelRatio();
  }
  else if (--m_line == 0)
  {
    m_line          = std::numeric_limits<int>::max();
    qint64 const ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
    int    const n  = (ms/1000) % m_period;
    auto   const t1 = DriftingDateTime::currentDateTimeUtc().addSecs(-n);
    auto   const ts = t1.toString(m_period < 60 ? "hh:mm:ss" : "hh:mm");

    p.setPen(Qt::white);
    p.drawText(5,
               p.fontMetrics().ascent(),
               QString("%1    %2").arg(ts).arg(m_band));
  }

  update();

  m_scaleOK = true;
}

// Draw the spectrum by copying the overlay prototype, then drawing the
// current array of points into it, up to the limit specified. If linear
// averaging has been requested for the spectrum, use a yellow line; any
// other type of spectral display gets a green line.

void
CPlotter::drawSpectrum(int const pointCount)
{
  m_SpectrumPixmap = m_OverlayPixmap.copy();

  QPainter p(&m_SpectrumPixmap);

  p.setPen(m_spectrum == Spectrum::LinearAvg ? Qt::yellow : Qt::green);
  p.drawPolyline(m_points.data(), pointCount);
}

void
CPlotter::drawDecodeLine(QColor const & color,
                         int    const   ia,
                         int    const   ib)
{
  auto const x1 = xFromFreq(ia);
  auto const x2 = xFromFreq(ib);

  QPainter p(&m_WaterfallPixmap);
  
  p.setPen(color);
  p.drawLine(qMin(x1, x2), 4, qMax(x1, x2), 4);
  p.drawLine(qMin(x1, x2), 0, qMin(x1, x2), 9);
  p.drawLine(qMax(x1, x2), 0, qMax(x1, x2), 9);
}

void
CPlotter::drawHorizontalLine(QColor const & color,
                             int    const   x,
                             int    const   width)
{
  QPainter p(&m_WaterfallPixmap);

  p.setPen(color);
  p.drawLine(x, 0, width <= 0 ? m_w : x + width, 0);
}

void
CPlotter::replot()
{
  resizeEvent(nullptr);
  float swide[m_w];

  QScopedValueRollback scoped(m_replot, true);

  for (int irow = 0; irow < m_h1; irow++)
  {
    m_j = irow;
    plotsave_(swide, &m_w, &m_h1, &irow);
    draw(swide, false);
  }

  update();
}

void
CPlotter::drawOverlay()
{
  if (m_OverlayPixmap.isNull()) return;

  QLinearGradient gradient(0, 0, 0, m_h2);

  gradient.setColorAt(1, Qt::black);
  gradient.setColorAt(0, Qt::darkBlue);

  QPainter p(&m_OverlayPixmap);

  p.setBrush(gradient);
  p.drawRect(0, 0, m_w, m_h2);
  p.setBrush(Qt::SolidPattern);

  auto        const fSpan = m_w * m_freqPerPixel;
  auto        const fpd   = freqPerDiv(fSpan);
  float       const ppdV  = fpd / m_freqPerPixel;
  float       const ppdH  = (float)m_h2 / VERT_DIVS; 
  std::size_t const hdivs = fSpan / fpd + 1.9999;
  auto        const x0    = static_cast<int>(fractionalPart((double)m_startFreq / fpd) * ppdV + 0.5);

  p.setPen(QPen(Qt::white, 1, Qt::DotLine));

  // Draw vertical grids.

  for (std::size_t i = 1; i < hdivs; i++)
  {
    if (auto const x  = static_cast<int>(i * ppdV) - x0;
                   x >= 0 &&
                   x <= m_w)
    {
      p.drawLine(x, 0, x , m_h2);
    }
  }

  // Draw horizontal grids.

  for (std::size_t i = 1; i < VERT_DIVS; i++)
  {
    auto const y = static_cast<int>(i * ppdH);
    p.drawLine(0, y, m_w, y);
  }

  drawScale(fpd, ppdV, hdivs);
}

void
CPlotter::drawScale(int         const fpd,
                    float       const ppdV,
                    std::size_t const hdivs)
{
  QPen const penOrange     (QColor(230, 126,  34), 3);
  QPen const penGray       (QColor(149, 165, 166), 3);
  QPen const penLightGreen (QColor( 46, 204, 113), 3);
  QPen const penLightYellow(QColor(241, 196,  15), 3);

  m_ScalePixmap.fill(Qt::white);
  QPainter p(&m_ScalePixmap);

  p.setFont(QFont("Arial"));
  p.setPen(Qt::black);
  p.drawRect(0, 0, m_w, 30);

  int         const fOffset = ((m_startFreq + fpd - 1) / fpd) * fpd;
  double      const xOffset = double(fOffset - m_startFreq) / fpd;
  std::size_t const nMajor  = hdivs - 1;
  std::size_t const nMinor  = fpd == 200 ? 4: 5;
  float       const ppdVM   = ppdV / nMinor;
  float       const ppdVL   = ppdV / 2;

  // Draw ticks and labels.

  for (std::size_t iMajor = 0; iMajor < nMajor; iMajor++)
  {
    auto const rMajor = (xOffset + iMajor) * ppdV;
    auto const xMajor = static_cast<int>(rMajor);
    p.drawLine(xMajor, 18, xMajor, 30);

    for (std::size_t iMinor = 1; iMinor < nMinor; iMinor++)
    {
      auto const xMinor = static_cast<int>(rMajor + iMinor * ppdVM);
      p.drawLine(xMinor, 22, xMinor, 30);
    }

    if (xMajor > 70)
    {
       p.drawText(QRect(xMajor - static_cast<int>(ppdVL), 0, static_cast<int>(ppdV), 20),
                  Qt::AlignCenter,
                  QString::number(fOffset + iMajor * fpd));
    }
  }

  // Given a starting frequency and range to cover, return corresponding
  // X values for the sub-band.

  auto const bandX = [this](float const start,
                            int   const range)
  {
    return std::make_pair(xFromFreq(start),
                          xFromFreq(start + range));
  };

  // Given a pair of X values, draw a band line, if visible.

  auto const drawBand = [this, &p](auto const & bandX)
  {
    auto const [x1, x2] = bandX;

    if (x1 <= m_w && x2 > 0)
    {
      p.drawLine(x1 + 1, 26, x2 - 2, 26);
      p.drawLine(x1 + 1, 28, x2 - 2, 28);
    }
  };

  // Colorize the JS8 sub-bands.

  p.setPen(penGray);        drawBand(bandX(   0.0f, 4000));
  p.setPen(penLightYellow); drawBand(bandX( 500.0f, 2500));
  p.setPen(penLightGreen);  drawBand(bandX(1000.0f, 1500));

  // If we're in the 30 meter band, we'd rather that the WSPR sub-band not
  // get stomped on; draw an orange indicator in the scale to denote the
  // WSPR portion of the band.
  //
  // Note that given the way XfromFreq() works, we're always going to see
  // clamped X values here, either 0 or m_w, if the frequency is outside
  // of the range, so we're always going to draw. If the WSPR range is not
  // in the displayed range, the effect will be, given the pen size, that
  // an orange indicator will indicate in which direction the WSPR range
  // lies.

  if (in30MBand())
  {
    auto const wspr = bandX(1.0e6f * (WSPR_START - m_dialFreq), WSPR_RANGE);

    p.setPen(penOrange);
    p.setFont(QFont("Arial", 10, QFont::Bold));
    drawBand(wspr);
    p.drawText(QRect(wspr.first, 0, wspr.second - wspr.first, 25),
               Qt::AlignHCenter|Qt::AlignBottom,
               "WSPR");
  }

  // Thin black line below the sub-band indicators; our work is done here. 

  p.setPen(Qt::black);
  p.drawLine(0, 29, m_w, 29);
}

// Draw the filter overlay pixmaps, if the filter is enabled and has a width
// greater than zero. Note that we could be more clever here and ensure the
// filter is actually visible prior to painting, but what we're doing here
// is reasonably trivial, so probably not worth the effort.

void
CPlotter::drawFilter()
{
  if (m_filterEnabled && m_filterWidth > 0 && !size().isEmpty())
  {
    auto const filterPixmap = [height = size().height(),
                               fill   = QColor(0, 0, 0, std::clamp(m_filterOpacity, 0, 255)),
                               dpr    = devicePixelRatio()](int const width,
                                                            int const lineX)
    {
      // Ending up with an unusable size here is expected, as in the case
      // where the combination of the filter center and width shifts one
      // or both ends of the filter out of the displayed range. Thus, no
      // matter what, we're going to return a pixmap here, though it may
      // be an empty one.

      if (auto const size = QSize(width, height);
                     size.isEmpty())
      {
        return QPixmap();
      }
      else
      {
        QPixmap pixmap = QPixmap(size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(fill);

        QPainter p(&pixmap);

        p.setPen(Qt::yellow);
        p.drawLine(lineX, 1, lineX, height);

        return pixmap;
      }
    };

    auto const width = m_filterWidth / 2.0f;
    auto const start = xFromFreq(m_filterCenter - width);
    auto const end   = xFromFreq(m_filterCenter + width);

    m_FilterPixmap = {
      filterPixmap(start, start),
      filterPixmap(size().width() - end, 0)
    };
  }
}

// Draw the two dials, the first of which will be used to display the selected
// offset and bandwith, the second prospective offset and bandwidth. These are
// not reliant on anything but height, submode, and bins per pixel.

void
CPlotter::drawDials()
{
  if (auto const height = size().height() - 30;
                 height > 0)
  {
    auto const width      = static_cast<int>(JS8::Submode::bandwidth(m_nSubMode) / m_freqPerPixel + 0.5);
    auto const dialPixmap = [size = QSize(width, height),
                             rect = QRect(1, 1, width - 2, height - 2),
                             dpr  = devicePixelRatio()](QColor const & color,
                                                        QBrush const & brush)
    {
      QPixmap pixmap = QPixmap(size * dpr);
      pixmap.setDevicePixelRatio(dpr);
      pixmap.fill(Qt::transparent);

      QPainter p(&pixmap);

      p.setBrush(brush);
      p.setPen(QPen(QBrush(color), 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
      p.drawRect(rect);

      return pixmap;
    };

    m_DialPixmap = {
      dialPixmap(Qt::red,   QBrush(QColor(255, 255, 255, 75), Qt::Dense4Pattern)),
      dialPixmap(Qt::white, Qt::transparent)
    };
  }
}

bool
CPlotter::in30MBand() const
{
  return (m_dialFreq >= BAND_30M_START &&
          m_dialFreq <= BAND_30M_END);
}

int
CPlotter::xFromFreq(float const f) const
{
  return std::clamp(static_cast<int>((f - m_startFreq) / m_freqPerPixel + 0.5), 0, m_w);
}

float
CPlotter::freqFromX(int const x) const
{
  return m_startFreq + x * m_freqPerPixel;
}

void
CPlotter::leaveEvent(QEvent * event)
{
  m_lastMouseX = -1;
  event->ignore();
}

void
CPlotter::wheelEvent(QWheelEvent * event)
{
    auto const y = event->angleDelta().y();

    if (auto const d = ((y > 0) - (y < 0)))
    {
      emit changeFreq(event->modifiers() & Qt::ControlModifier
                    ? freq()           + d
                    : freq() / 10 * 10 + d * 10);
    }
    else
    {
      event->ignore();
    }
}

void
CPlotter::mouseMoveEvent(QMouseEvent * event)
{
  m_lastMouseX = std::clamp(static_cast<int>(event->position().x()), 0, m_w);

  update();
  event->ignore();

  QToolTip::showText(event->globalPosition().toPoint(),
                     QString::number(static_cast<int>(freqFromX(m_lastMouseX))));
}

void
CPlotter::mouseReleaseEvent(QMouseEvent * event)
{
  if (Qt::LeftButton == event->button())
  {
    emit changeFreq(static_cast<int>(freqFromX(m_lastMouseX)));
  }
  else
  {
    event->ignore();
  }
}

void
CPlotter::setBand(QString const & band)
{
  if (m_band != band)
  {
    m_band = band;
    update();
  }
}

void
CPlotter::setBinsPerPixel(int const binsPerPixel)
{
  if (m_binsPerPixel != binsPerPixel)
  {
    m_binsPerPixel = std::max(1, binsPerPixel);
    m_freqPerPixel = m_binsPerPixel * FFT_BIN_WIDTH;
    drawOverlay();
    drawFilter();
    drawDials();
    update();
  }
}

void
CPlotter::setDialFreq(float const dialFreq)
{
  if (m_dialFreq != dialFreq)
  {
    m_dialFreq = dialFreq;
    drawOverlay();
    update();
  }
}

void
CPlotter::setFilter(int const filterCenter,
                    int const filterWidth)
{
  if (m_filterCenter != filterCenter ||
      m_filterWidth  != filterWidth)
  {
    m_filterCenter = filterCenter;
    m_filterWidth  = filterWidth;
    drawFilter();
    update();
  }
}

void
CPlotter::setFilterEnabled(bool const filterEnabled)
{
  if (m_filterEnabled != filterEnabled)
  {
    m_filterEnabled = filterEnabled;
    drawFilter();
    update();
  }
}

void
CPlotter::setFilterOpacity(int const filterOpacity)
{
  if (m_filterOpacity != filterOpacity)
  {
    m_filterOpacity = filterOpacity;
    drawFilter();
    update();
  }
}

void
CPlotter::setFreq(int const freq)
{
  if (m_freq != freq)
  {
    m_freq = freq;
    drawOverlay();
    update();
  }
}

void
CPlotter::setPercent2DScreen(int percent2DScreen)
{
  if (m_percent2DScreen != percent2DScreen)
  {
    m_percent2DScreen = percent2DScreen;
    resizeEvent(nullptr);
    update();
  }
}

void
CPlotter::setPeriod(int const period)
{
  if (m_period != period)
  {
    m_period = period;
    update();
  }
}

void
CPlotter::setPlot2dGain(int const plot2dGain)
{
  if (m_plot2dGain != plot2dGain)
  {
    m_plot2dGain = plot2dGain;
    update();
  }
}

void
CPlotter::setStartFreq(int const startFreq)
{
  if (m_startFreq != startFreq)
  {
    m_startFreq = startFreq;
    drawOverlay();
    drawFilter();
    update();
  }
}

void
CPlotter::setSubMode(int const nSubMode)
{
  if (m_nSubMode != nSubMode)
  {
    m_nSubMode = nSubMode;
    drawDials();
    update();
  }
}
