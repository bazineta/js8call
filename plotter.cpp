#include "plotter.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <type_traits>
#include <utility>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QToolTip>
#include <QWheelEvent>
#include "commons.h"
#include "moc_plotter.cpp"
#include "DriftingDateTime.h"
#include "JS8Submode.hpp"

extern "C" {
  void flat4_(float swide[], int* iz, bool* bflatten);
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

  auto
  freqPerDiv(double const fSpan)
  {
    if (fSpan > 2500) { return 500; }
    if (fSpan > 1000) { return 200; }
    if (fSpan >  500) { return 100; }
    if (fSpan >  250) { return  50; }
    if (fSpan >  100) { return  20; }
                        return  10;
  }

  // Return text for a decode line that occurred now.

  auto
  decodeLineText(int     const   period,
                 QString const & band)
  {
    auto const now = DriftingDateTime::currentDateTimeUtc();
    auto const ms  = now.toMSecsSinceEpoch() % 86400000;
    auto const ts  = now.addSecs(-(ms / 1000) % period);

    return QString("%1    %2")
                  .arg(ts.toString(period < 60 ? "hh:mm:ss" : "hh:mm"))
                  .arg(band);
  }

  // Given a spectrum, return an appropriate pen to draw it.

  auto
  spectrumPen(CPlotter::Spectrum const spectrum)
  {
    switch (spectrum)
    {
      case CPlotter::Spectrum::Current:    return Qt::green;
      case CPlotter::Spectrum::Cumulative: return Qt::cyan;
      case CPlotter::Spectrum::LinearAvg:  return Qt::yellow;
    }
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
  setAttribute(Qt::WA_OpaquePaintEvent);
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
    m_h2   = m_percent2DScreen * (m_h - 30) / 100.0;
    m_h1   = m_h - m_h2;

    // We want our 3 main pixmaps sized to occupy our entire height,
    // and to be completely filled with an opaque color, since we're
    // going to take the opaque paint even optimization path. If this
    // is a high-DPI display, scale the pixmaps to avoid text looking
    // pixelated.

    m_ScalePixmap     = makePixmap({m_w,   30}, Qt::white);
    m_WaterfallPixmap = makePixmap({m_w, m_h1}, Qt::black);
    m_OverlayPixmap   = makePixmap({m_w, m_h2}, Qt::black);

    // The replot circular buffer should have capacity to hold the full
    // height of the waterfall pixmap, in device, not logical, pixels.

    m_replot = Replot(m_WaterfallPixmap.size().height());

    // While the dials and filter are parameterized, they don't depend
    // on inbound data, so we can draw them now.

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

  p.drawPixmap(0, 0,    m_ScalePixmap);
  p.drawPixmap(0, 30,   m_WaterfallPixmap);
  p.drawPixmap(0, m_h1, m_SpectrumPixmap);

  p.drawPixmap(xFromFreq(m_freq), 30, m_DialPixmap[0]);

  if (m_lastMouseX >= 0)
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
               bool const bReplot)
{
  // Move current data down one line; we must do this before
  // attaching a QPainter.

  m_WaterfallPixmap.scroll(0, 1, m_WaterfallPixmap.rect());

  QPainter p(&m_WaterfallPixmap);

  if (!bReplot && swide[0] < 1.e29f)
  {
    flat4_(swide, &m_w, &m_flatten);
  }

  if(swide[0] > 1.e29f) p.setPen(Qt::green); // horizontal line

  if (!bReplot)
  {
    m_replot.push_back({swide, swide + m_w});
  }

  auto const fac  = std::sqrt(m_binsPerPixel * m_waterfallAvg / 15.0f);
  auto const gain = 10.0f * fac * std::pow(10.0f, 0.015f * m_plotGain);
  auto       ymin = 1.e30f;

  // First loop; draws points into the waterfall and determines the
  // minimum y extent.

  for (int i = 0; i < m_w; i++)
  {
    float const y = swide[i];
    if (y < ymin  ) ymin = y;
    if (y < 1.e29f) p.setPen(m_colors[std::clamp(m_plotZero + static_cast<int>(gain * y), 0, 254)]);
    p.drawPoint(i, 0);
  }

  // If this is a replot, then we're done here; we don't want to
  // proceed to spectral analysis; our mission was just to redraw
  // the waterfall. Note that we're also not going to add back the
  // decode line text, which isn't ideal.

  if (bReplot) return;

  // If we've just drawn a decode line, compute the number of lines required
  // before we need to draw the decode text. If that wasn't a decode line,
  // see if we've reached the point where we should draw the decode text.

  if (swide[0] > 1.e29f)
  {
    m_line = p.fontMetrics().height() * devicePixelRatio();
    m_text = decodeLineText(m_period, m_band);
  }
  else if (--m_line == 0)
  {
    m_line = std::numeric_limits<int>::max();

    p.setPen(Qt::white);
    p.drawText(5, p.fontMetrics().ascent(), m_text);
  }

  // If the spectrum is of zero height, we're done here.

  if (!m_h2)
  {
    update();
    return;
  }

  // Summarization method, used for computation of cumulative and
  // linear average data.

  auto const sum = [base = static_cast<int>(m_startFreq / FFT_BIN_WIDTH + 0.5),
                    bins = m_binsPerPixel](float const * const data,
                                           auto  const         index)
  {
    auto const offset = data + base + bins * index;

    return std::accumulate(offset, offset + bins, 0.0f) / bins;
  };

  // Clear the current points and ensure space exists to add all the
  // points we require without reallocation.

  m_points.clear();
  m_points.reserve(m_w);

  // Compute gain for the spectrum.

  auto const gain2d = std::pow(10.0f, 0.02f * m_plot2dGain);

  // Second loop, determines how we're going to draw the spectrum.

  for (int i = 0; i < m_w; i++)
  {
    float y = 0;

    switch (m_spectrum)
    {
      case Spectrum::Current:
        y = gain2d * (swide[i] - ymin) + m_plot2dZero + (m_flatten ? 0 : 15);
      break;
      case Spectrum::Cumulative:
        y = gain2d * (sum(dec_data.savg, i) + m_plot2dZero) + (m_flatten ? 0 : 15);
      break;
      case Spectrum::LinearAvg:
        y = gain2d * sum(spectra_.syellow, i) + m_plot2dZero;
      break;
    }

    m_points.emplace_back(i, static_cast<int>(0.9f * m_h2 - y * m_h2 / 70.0f));
  }

  drawSpectrum();
}

// Draw the spectrum by copying the overlay prototype, then drawing the
// current array of points into it, up to the limit specified. If linear
// averaging has been requested for the spectrum, use a yellow line; any
// other type of spectral display gets a green line.

void
CPlotter::drawSpectrum()
{
  m_SpectrumPixmap = m_OverlayPixmap.copy();

  QPainter p(&m_SpectrumPixmap);

  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(spectrumPen(m_spectrum));
  p.drawPolyline(m_points);

  update();
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
CPlotter::drawOverlay()
{
  auto        const fSpan = m_w * m_freqPerPixel;
  auto        const fpd   = freqPerDiv(fSpan);
  float       const ppdV  = fpd / m_freqPerPixel;
  std::size_t const hdivs = fSpan / fpd + 1.9999;

  drawScale(fpd, ppdV, hdivs);

  if (m_OverlayPixmap.isNull()) return;

  QLinearGradient gradient(0, 0, 0, m_h2);

  gradient.setColorAt(1, Qt::black);
  gradient.setColorAt(0, Qt::darkBlue);

  QPainter p(&m_OverlayPixmap);

  p.setBrush(gradient);
  p.drawRect(0, 0, m_w, m_h2);
  p.setBrush(Qt::SolidPattern);

  p.setPen(QPen(Qt::darkGray, 1, Qt::DotLine));

  // Draw vertical grids.

  auto const x0 = static_cast<int>(fractionalPart((double)m_startFreq / fpd) * ppdV + 0.5);

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
  
  float const ppdH = (float)m_h2 / VERT_DIVS; 

  for (std::size_t i = 1; i < VERT_DIVS; i++)
  {
    auto const y = static_cast<int>(i * ppdH);
    p.drawLine(0, y, m_w, y);
  }
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

void
CPlotter::replot()
{
  m_WaterfallPixmap.fill(Qt::black);

  for (auto & entry : m_replot)
  {
    draw(entry.data(), true);
  }

  update();
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
CPlotter::setColors(Colors const & colors)
{
  if (m_colors != colors)
  {
    m_colors = colors;
    replot();
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
CPlotter::setPlotGain(int const plotGain)
{
  if (m_plotGain != plotGain)
  {
    m_plotGain = plotGain;
    replot();
  }
}

void
CPlotter::setPlotZero(int const plotZero)
{
  if (m_plotZero != plotZero)
  {
    m_plotZero = plotZero;
    replot();
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
