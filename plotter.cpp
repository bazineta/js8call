#include "plotter.h"
#include <algorithm>
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

  constexpr double BAND_30M_START = 10.13;
  constexpr double BAND_30M_END   = 10.15;
  
  // The WSPR range is 200Hz in the 30m band, starting at 10.1401 MHz.

  constexpr double WSPR_RANGE = 200.0;
  constexpr double WSPR_START = 10.1401;

  // Vertical divisions in the spectrum display.

  constexpr std::size_t VERT_DIVS = 7;

  int
  freqPerDiv(float const fSpan)
  {
    if (fSpan > 2500) { return 500; }
    if (fSpan > 1000) { return 200; }
    if (fSpan >  500) { return 100; }
    if (fSpan >  250) { return  50; }
    if (fSpan >  100) { return  20; }
                        return  10;
  }

  double
  fftBinWidth(qint32 const nsps)
  {
    switch (nsps)
    {
      case 252000: return 1500.0 / 32768.0;
      case  82944: return 1500.0 / 12288.0;
      case  40960: return 1500.0 /  6144.0;
      default:     return 1500.0 /  2048.0;
    }
  }
}

CPlotter::CPlotter(QWidget * parent)
  : QFrame {parent}
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_PaintOnScreen,false);
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAttribute(Qt::WA_NoSystemBackground, true);
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
CPlotter::resizeEvent(QResizeEvent *)                    //resizeEvent()
{
  if (!size().isValid()) return;

  if ((m_size            != size())        ||
      (m_Percent2DScreen != m_Percent2DScreen0))
  {
    m_size = size();
    m_w    = m_size.width();
    m_h    = m_size.height();
    m_h2   = m_Percent2DScreen * m_h / 100.0;
    
    if (m_h2 > m_h - 30) m_h2 = m_h - 30;
    if (m_h2 <        1) m_h2 =        1;
    
    m_h1 = m_h - m_h2;
//    m_line=0;

    m_FilterOverlayPixmap = QPixmap(m_size);
    m_FilterOverlayPixmap.fill(Qt::transparent);

    m_DialOverlayPixmap = QPixmap(m_size);
    m_DialOverlayPixmap.fill(Qt::transparent);

    m_HoverOverlayPixmap = QPixmap(m_size);
    m_HoverOverlayPixmap.fill(Qt::transparent);

    m_2DPixmap = QPixmap(m_w, m_h2);
    m_2DPixmap.fill(Qt::black);

    m_WaterfallPixmap = QPixmap(m_w, m_h1);
    m_WaterfallPixmap.fill(Qt::black);

    m_OverlayPixmap = QPixmap(m_w, m_h2);
    m_OverlayPixmap.fill(Qt::black);

    // Address scale font looking terrible, since it's drawn into this
    // intermediate pixmap, so if we don't scale it to match the device,
    // the text will look pixelated.
    //
    // Same is true of the decode lines in the waterfall; they look
    // pixelated, but the fix doesn't appear to be straightforward, and
    // it's arguably an effect there, a bit like a Tektronix display.

    m_ScalePixmap = QPixmap(QSize(m_w, 30) * devicePixelRatio());
    m_ScalePixmap.setDevicePixelRatio(devicePixelRatio());
    m_ScalePixmap.fill(Qt::white);

    m_Percent2DScreen0 = m_Percent2DScreen;
  }
  drawOverlay();
}

void
CPlotter::paintEvent(QPaintEvent *)                                // paintEvent()
{
  if (m_paintEventBusy) return;

  QScopedValueRollback scoped(m_paintEventBusy, true);
  QPainter             painter(this);

  painter.drawPixmap(0,    0, m_ScalePixmap);
  painter.drawPixmap(0,   30, m_WaterfallPixmap);
  painter.drawPixmap(0, m_h1, m_2DPixmap);

  auto const x = xFromFreq(m_rxFreq);

  painter.drawPixmap(x, 0, m_DialOverlayPixmap);

  if (m_lastMouseX >= 0 &&
      m_lastMouseX != x)
  {
    painter.drawPixmap(m_lastMouseX, 0, m_HoverOverlayPixmap);
  }

  if (m_filterEnabled && m_filterWidth > 0)
  {
    painter.drawPixmap(0, 0, m_FilterOverlayPixmap);
  }
}

void
CPlotter::draw(float      swide[],
               bool const bScroll)
{
  // Move current data down one line (must do this before attaching a QPainter object)

  if(bScroll && !m_replot)
  {
    m_WaterfallPixmap.scroll(0, 1, m_WaterfallPixmap.rect());
  }

  QPainter painter1(&m_WaterfallPixmap);
  m_2DPixmap = m_OverlayPixmap.copy();
  QPainter painter2D(&m_2DPixmap);
  if(!painter2D.isActive()) return;

  auto iz = xFromFreq(5000.0);

  if (bScroll && swide[0] < 1.e29)
  {
    flat4_(swide, &iz, &m_flatten);
  }

  if(swide[0] > 1.e29 && swide[0] < 1.5e30) painter1.setPen(Qt::green); // horizontal line
  if(swide[0] > 1.4e30                    ) painter1.setPen(Qt::yellow);

  if(!m_replot)
  {
    m_j      =  0;
    int irow = -1;

    plotsave_(swide, &m_w, &m_h1, &irow);
  }

  double const fac    = sqrt(m_binsPerPixel * m_waterfallAvg / 15.0);
  double const gain   = fac * pow(10.0, 0.015 * m_plotGain);
  double const gain2d =       pow(10.0, 0.02  * m_plot2dGain);
  auto   const base   = static_cast<int>(m_startFreq / m_fftBinWidth + 0.5);
  auto         ymin   = 1.e30f;

  // First loop; draws points into the waterfall and determines the
  // minimum y extent.

  for(int i = 0; i < iz; i++)
  {
    float const y = swide[i];
    if (y < ymin ) ymin = y;
    if (y < 1.e29) painter1.setPen(g_ColorTbl[std::clamp(static_cast<int>(10.0 * gain * y + m_plotZero), 0, 254)]);
    painter1.drawPoint(i, m_j);
  }

  m_line++;

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

  // Draw the computed spectrum line.

  painter2D.setPen(m_spectrum == Spectrum::LinearAvg ? Qt::yellow : Qt::green);
  painter2D.drawPolyline(m_points.data(), iz - 1);

  if (m_replot) return;

  if (swide[0] > 1.0e29) m_line = 0;
  if (m_line == painter1.fontMetrics().height())
  {
    qint64 const ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
    int    const n  = (ms/1000) % m_TRperiod;
    auto   const t1 = DriftingDateTime::currentDateTimeUtc().addSecs(-n);
    auto   const ts = t1.toString(m_TRperiod < 60 ? "hh:mm:ss" : "hh:mm");

    painter1.setPen(Qt::white);
    painter1.drawText(5,
                      painter1.fontMetrics().ascent(),
                      QString("%1    %2").arg(ts).arg(m_band));
  }

  update();                                    //trigger a new paintEvent

  m_scaleOK = true;
}

void
CPlotter::drawDecodeLine(QColor const & color,
                         int    const   ia,
                         int    const   ib)
{
  auto const x1 = xFromFreq(ia);
  auto const x2 = xFromFreq(ib);

  QPainter painter1(&m_WaterfallPixmap);
  
  painter1.setPen(color);
  painter1.drawLine(qMin(x1, x2), 4, qMax(x1, x2), 4);
  painter1.drawLine(qMin(x1, x2), 0, qMin(x1, x2), 9);
  painter1.drawLine(qMax(x1, x2), 0, qMax(x1, x2), 9);
}

void
CPlotter::drawHorizontalLine(QColor const & color,
                             int    const   x,
                             int    const   width)
{
  QPainter painter1(&m_WaterfallPixmap);

  painter1.setPen(color);
  painter1.drawLine(x, 0, width <= 0 ? m_w : x + width, 0);
}

void
CPlotter::replot()
{
  resizeEvent(nullptr);
  float swide[m_w];

  m_replot = true;

  for (int irow = 0; irow < m_h1; irow++)
  {
    m_j = irow;
    plotsave_(swide, &m_w, &m_h1, &irow);
    draw(swide, false);
  }

  update();                                    //trigger a new paintEvent

  m_replot = false;
}

void
CPlotter::drawOverlay()
{
  if (m_OverlayPixmap.isNull() ||
      m_WaterfallPixmap.isNull()) return;

  QLinearGradient gradient(0, 0, 0, m_h2);     //fill background with gradient

  gradient.setColorAt(1, Qt::black);
  gradient.setColorAt(0, Qt::darkBlue);

  QPainter p(&m_OverlayPixmap);

  p.setBrush(gradient);
  p.drawRect(0, 0, m_w, m_h2);
  p.setBrush(Qt::SolidPattern);

  double const df = m_binsPerPixel * m_fftBinWidth;

  m_fSpan        = m_w * df;
  auto const fpd = freqPerDiv(m_fSpan);

  float       const ppdV  = fpd / df;
  float       const ppdH  = (float)m_h2 / (float)VERT_DIVS; 
  std::size_t const hdivs = m_fSpan / fpd + 1.9999;

  float xx0 = float(m_startFreq) /float(fpd);
  xx0 = xx0 - int(xx0);
  int x0 = xx0 * ppdV + 0.5;

  p.setPen(QPen(Qt::white, 1, Qt::DotLine));

  for (std::size_t i = 1; i < hdivs; i++)  //draw vertical grids
  {
    if (int const x  = (int)((float)i * ppdV) - x0;
                  x >= 0 &&
                  x <= m_w)
    {
      p.drawLine(x, 0, x , m_h2);
    }
  }

  for (std::size_t i = 1; i < VERT_DIVS; i++)  //draw horizontal grids
  {
    int const y = (int)( (float)i * ppdH );
    p.drawLine(0, y, m_w, y);
  }

  drawOverlayScale(df, fpd, ppdV);

  // paint dials and filter overlays
  if (m_mode == "FT8")
  {
    auto const fwidth = xFromFreq(m_rxFreq + JS8::Submode::bandwidth(m_nSubMode)) - xFromFreq(m_rxFreq);

    drawOverlayDial(fwidth);
    drawOverlayHover(fwidth);
    drawOverlayFilter();
  }
}

void
CPlotter::drawOverlayScale(double const df,
                           int    const fpd,
                           float  const ppdV)
{
  QPen const penOrange     (QColor(230, 126,  34), 3);
  QPen const penGray       (QColor(149, 165, 166), 3);
  QPen const penLightGreen (QColor( 46, 204, 113), 3);
  QPen const penLightYellow(QColor(241, 196,  15), 3);

  m_ScalePixmap.fill(Qt::white);
  QPainter p(&m_ScalePixmap);

  p.setFont({"Arial"});
  p.setPen(Qt::black);
  p.drawRect(0, 0, m_w, 30);

  int         const fOffset = ((m_startFreq + fpd - 1) / fpd) * fpd;
  double      const xOffset = double(fOffset - m_startFreq) / fpd;
  std::size_t const nMinor  = fpd == 200 ? 4: 5;
  std::size_t const nHDivs  = m_w * df / fpd + 0.9999;
  float       const ppdVM   = ppdV / nMinor;
  float       const ppdVL   = ppdV / 2;

  // Draw ticks and labels.

  for (std::size_t iMajor = 0; iMajor < nHDivs; iMajor++)
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

  // Colorize the JS8 sub-bands.

  for (std::size_t i = 0; i <= 3500; i += 500)
  {
    auto const x1 = xFromFreq(static_cast<float>(i));
    auto const x2 = xFromFreq(static_cast<float>(i + 500));

    if (x1 <= m_w && x2 > 0)
    {
      switch (i) {
      case 500:
      case 2500:
        p.setPen(penLightYellow);
        break;
      case 1000:
      case 1500:
      case 2000:
        p.setPen(penLightGreen);
        break;
      default:
        p.setPen(penGray);
        break;
      }
      p.drawLine(x1 + 1, 26, x2 - 2, 26);
      p.drawLine(x1 + 1, 28, x2 - 2, 28);
    }
  }

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
    auto const wspr = 1.0e6 * (WSPR_START - m_dialFreq);
    auto const x1   = xFromFreq(wspr);
    auto const x2   = xFromFreq(wspr + WSPR_RANGE);

    p.setPen(penOrange);
    p.setFont({"Arial", 10, QFont::Bold});
    p.drawLine(x1 + 1, 26, x2 - 2, 26);
    p.drawLine(x1 + 1, 28, x2 - 2, 28);
    p.drawText(QRect(x1, 0, x2 - x1, 25),
               Qt::AlignHCenter|Qt::AlignBottom,
               "WSPR");
  }

  // Thin black line below the sub-band indicators; our work is done here. 

  p.setPen(Qt::black);
  p.drawLine(0, 29, m_w, 29);
}

// Paint the dial overlay, showing the chunk of the frequency spectrum
// presently in use.

void
CPlotter::drawOverlayDial(int const fwidth)
{
  QPainter p(&m_DialOverlayPixmap);

  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(rect(), Qt::transparent);
  p.setPen(Qt::red);
  p.fillRect(0,          26, fwidth + 2, 4, Qt::red);
  p.fillRect(0,     m_h - 4, fwidth + 2, 4, Qt::red);
  p.drawLine(0,          30, 0,          m_h); // first slot, left line
  p.drawLine(fwidth + 1, 30, fwidth + 1, m_h); // first slot, right line
}

// Paint the hover overlay, showing the prospective chunk of frequency
// spectrum under the mouse.

void
CPlotter::drawOverlayHover(int const fwidth)
{
  QPainter p(&m_HoverOverlayPixmap);

  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(rect(), Qt::transparent);
  p.setPen(Qt::white);
  p.drawLine(0,      30, 0,      m_h); // first slot, left line hover
  p.drawLine(fwidth, 30, fwidth, m_h); // first slot, right line hover
}

// Paint the filter overlay pixmap, if the filter is enabled and has a width
// greater than zero. Note that we could be more clever here and ensure the
// filter is actually visible prior to painting, but what we're doing here
// is reasonably trivial, so probably not worth the effort.

void
CPlotter::drawOverlayFilter()
{
  if (m_filterEnabled && m_filterWidth > 0)
  {
    QPainter p(&m_FilterOverlayPixmap);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);

    auto const start = xFromFreq(static_cast<float>(m_filterCenter - m_filterWidth / 2));
    auto const end   = xFromFreq(static_cast<float>(m_filterCenter + m_filterWidth / 2));

    // Yellow vertical line, showing the filter location.

    p.setPen(Qt::yellow);
    p.drawLine(start, 30, start, m_h);
    p.drawLine(end,   30, end,   m_h);

    // Put a mask over everything outside the bandpass.

    QColor const blackMask(0, 0, 0, std::clamp(m_filterOpacity, 0, 255));

    p.fillRect(0,       30, start, m_h, blackMask);
    p.fillRect(end + 1, 30, m_w,   m_h, blackMask);
  }
}

bool
CPlotter::in30MBand() const
{
  return (m_dialFreq >= BAND_30M_START &&
          m_dialFreq <= BAND_30M_END);
}

int
CPlotter::xFromFreq(float const f) const               //XfromFreq()
{
  return std::clamp(static_cast<int>(m_w * (f - m_startFreq) / m_fSpan + 0.5), 0, m_w);
}

float
CPlotter::freqFromX(int const x) const               //FreqfromX()
{
  return float(m_startFreq + x * m_binsPerPixel * m_fftBinWidth);
}

void
CPlotter::setPlot2dGain(int const n)                 //setPlot2dGain
{
  m_plot2dGain = n;
  update();
}

void
CPlotter::setStartFreq(int const f)                  //SetStartFreq()
{
  m_startFreq = f;
  resizeEvent(nullptr);
  drawOverlay();
  update();
}

void
CPlotter::setBinsPerPixel(int const n)             //setBinsPerPixel
{
  m_binsPerPixel = std::max(1, n);
  drawOverlay();                         //Redraw scales and ticks
  update();                              //trigger a new paintEvent}
}

void
CPlotter::setRxFreq(int const x)                   //setRxFreq
{
  m_rxFreq = x;         // x is freq in Hz
  drawOverlay();
  update();
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
    auto const delta = event->angleDelta();

    if (delta.isNull())
    {
      event->ignore();
      return;
    }

    int const dir     = delta.y() > 0 ? 1 : -1;
    int       newFreq = rxFreq();

    if(event->modifiers() & Qt::ControlModifier)
    {
        newFreq += dir;
    }
    else
    {
        newFreq = newFreq / 10 * 10 + dir * 10;
    }

    emit setFreq1(newFreq, newFreq);
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
    auto const x         = std::clamp(static_cast<int>(event->position().x()), 0, m_w);
    bool const ctrl      = event->modifiers() & Qt::ControlModifier;
    bool const shift     = event->modifiers() & Qt::ShiftModifier;
    auto const newFreq   = static_cast<int>(freqFromX(x) + 0.5);
    int  const oldTxFreq = m_txFreq;
    int  const oldRxFreq = m_rxFreq;
    
    if      (ctrl)  { emit setFreq1(newFreq,   newFreq  ); }
    else if (shift) { emit setFreq1(oldRxFreq, newFreq  ); }
    else            { emit setFreq1(newFreq,   oldTxFreq); }
  }
  else
  {
    event->ignore();           // let parent handle
  }
}

void
CPlotter::setNsps(int const ntrperiod,
                  int const nsps)                    //setNsps
{
  m_TRperiod    = ntrperiod;
  m_nsps        = nsps;
  m_fftBinWidth = fftBinWidth(nsps);

  drawOverlay(); //Redraw scales and ticks
  update();      //trigger a new paintEvent}
}

void
CPlotter::setTxFreq(int const n)                            //setTxFreq
{
  m_txFreq = n;
  drawOverlay();
  update();
}

void
CPlotter::setDialFreq(double const d)
{
  m_dialFreq = d;
  drawOverlay();
  update();
}

void
CPlotter::setBand(QString const & band)
{
  m_band = band;
  drawOverlay();
  update();
}

void
CPlotter::setFilterCenter(int const center)
{
  m_filterCenter = center;
  drawOverlay();
  update();
}

void
CPlotter::setFilterWidth(int const width)
{
  m_filterWidth = width;
  drawOverlay();
  update();
}

void
CPlotter::setFilterEnabled(bool const enabled)
{
  m_filterEnabled = enabled;
  drawOverlay();
  update();
}

void
CPlotter::setFilterOpacity(int const alpha)
{
  m_filterOpacity = alpha;
  drawOverlay();
  update();
}

void
CPlotter::setMode(QString const & mode)
{
  m_mode = mode;
  drawOverlay();
  update();
}

void
CPlotter::setSubMode(int const nSubMode)
{
  m_nSubMode = nSubMode;
  drawOverlay();
  update();
}

void
CPlotter::setPercent2DScreen(int percent)
{
  m_Percent2DScreen = percent;
  resizeEvent(nullptr);
  update();
}
