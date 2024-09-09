#include "plotter.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QToolTip>
#include <QWheelEvent>
#include "commons.h"
#include "moc_plotter.cpp"
#include <algorithm>
#include <fstream>
#include <iostream>

#include "DriftingDateTime.h"
#include "varicode.h"

#define MAX_SCREENSIZE 2048

extern "C" {
  void flat4_(float swide[], int* iz, int* nflatten);
  void plotsave_(float swide[], int* m_w , int* m_h1, int* irow);
}

namespace
{
  qint32
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

  float
  bw(qint32 const nSubMode)
  {
    switch (nSubMode)
    {
      case Varicode::JS8CallNormal: return 8.0 * (double)RX_SAMPLE_RATE / (double)JS8A_SYMBOL_SAMPLES;
      case Varicode::JS8CallFast:   return 8.0 * (double)RX_SAMPLE_RATE / (double)JS8B_SYMBOL_SAMPLES;
      case Varicode::JS8CallTurbo:  return 8.0 * (double)RX_SAMPLE_RATE / (double)JS8C_SYMBOL_SAMPLES;
      case Varicode::JS8CallSlow:   return 8.0 * (double)RX_SAMPLE_RATE / (double)JS8E_SYMBOL_SAMPLES;
      case Varicode::JS8CallUltra:  return 8.0 * (double)RX_SAMPLE_RATE / (double)JS8I_SYMBOL_SAMPLES;
      default:                      return 0;
    }
  }
}

CPlotter::CPlotter(QWidget *parent) :                  //CPlotter Constructor
  QFrame {parent},
  m_set_freq_action {new QAction {tr ("&Set Rx && Tx Offset"), this}},
  m_bScaleOK {false},
  m_bReference {false},
  m_bReference0 {false},
  m_fSpan {2000.0},
  m_plotZero {0},
  m_plotGain {0},
  m_plot2dGain {0},
  m_plot2dZero {0},
  m_nSubMode {0},
  m_filterEnabled{false},
  m_filterCenter {0},
  m_filterWidth {0},
  m_turbo {false},
  m_Running {false},
  m_paintEventBusy {false},
  m_fftBinWidth {1500.0/2048.0},
  m_dialFreq {0.},
  m_sum {},
  m_hdivs {HORZ_DIVS},
  m_line {0},
  m_nsps {6912},
  m_Percent2DScreen {0},      //percent of screen used for 2D display
  m_Percent2DScreen0 {0},
  m_rxFreq {1020},
  m_txFreq {0},
  m_startFreq {0},
  m_lastMouseX {-1}
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_PaintOnScreen,false);
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAttribute(Qt::WA_NoSystemBackground, true);
  m_bReplot=false;

  setMouseTracking(true);
}

CPlotter::~CPlotter() = default;

QSize CPlotter::minimumSizeHint() const
{
  return QSize(50, 50);
}

QSize CPlotter::sizeHint() const
{
  return QSize(180, 180);
}

void CPlotter::resizeEvent(QResizeEvent *)                    //resizeEvent()
{
  if (!size().isValid()) return;

  if ((m_Size            != size())        ||
      (m_bReference      != m_bReference0) ||
      (m_Percent2DScreen != m_Percent2DScreen0))
  {
    m_Size = size();
    m_w    = m_Size.width();
    m_h    = m_Size.height();
    m_h2   = m_Percent2DScreen * m_h / 100.0;
    
    if (m_h2 > m_h - 30) m_h2 = m_h - 30;
    if (m_bReference   ) m_h2 = m_h - 30;
    if (m_h2 <        1) m_h2 =        1;
    
    m_h1 = m_h - m_h2;
//    m_line=0;

    m_FilterOverlayPixmap = QPixmap(m_Size);
    m_FilterOverlayPixmap.fill(Qt::transparent);

    m_DialOverlayPixmap = QPixmap(m_Size);
    m_DialOverlayPixmap.fill(Qt::transparent);

    m_HoverOverlayPixmap = QPixmap(m_Size);
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
  DrawOverlay();
}

void CPlotter::paintEvent(QPaintEvent *)                                // paintEvent()
{
  if (m_paintEventBusy) return;

  m_paintEventBusy = true;
  
  QPainter painter(this);
  painter.drawPixmap(0,0,m_ScalePixmap);
  painter.drawPixmap(0,30,m_WaterfallPixmap);
  painter.drawPixmap(0,m_h1,m_2DPixmap);

  int const x = XfromFreq(m_rxFreq);
  painter.drawPixmap(x,0,m_DialOverlayPixmap);

  if (m_lastMouseX >= 0 && m_lastMouseX != x)
  {
    painter.drawPixmap(m_lastMouseX, 0, m_HoverOverlayPixmap);
  }

  if (m_filterEnabled && m_filterWidth > 0)
  {
    painter.drawPixmap(0, 0, m_FilterOverlayPixmap);
  }

  m_paintEventBusy = false;
}

void CPlotter::draw(float swide[], bool bScroll, bool)
{
  static int ktop   = 0;
  double     fac    = sqrt(m_binsPerPixel * m_waterfallAvg / 15.0);
  double     gain   = fac * pow(10.0, 0.015 * m_plotGain);
  double     gain2d =       pow(10.0, 0.02  * m_plot2dGain);
  float      y2;
  float      ymin;
  int        j;
  int        j0;

  if(m_bReference != m_bReference0) resizeEvent(nullptr);
  m_bReference0 = m_bReference;

  // Move current data down one line (must do this before attaching a QPainter object)

  if(bScroll && !m_bReplot)
  {
    m_WaterfallPixmap.scroll(0, 1, m_WaterfallPixmap.rect());
  }

  QPainter painter1(&m_WaterfallPixmap);
  m_2DPixmap = m_OverlayPixmap.copy();
  QPainter painter2D(&m_2DPixmap);
  if(!painter2D.isActive()) return;
  QFont Font("Arial");
  Font.setPointSize(12);
  Font.setWeight(QFont::Normal);
  painter2D.setFont(Font);

  if     (m_bLinearAvg) { painter2D.setPen(Qt::yellow); }
  else if(m_bReference) { painter2D.setPen(Qt::blue);   }
  else                  { painter2D.setPen(Qt::green);  }

  static QPoint LineBuf [MAX_SCREENSIZE];
  static QPoint LineBuf2[MAX_SCREENSIZE];

  j      = 0;
  j0     = int(m_startFreq/m_fftBinWidth + 0.5);
  int iz = XfromFreq(5000.0);
  m_fMax = FreqfromX(iz);

  if(bScroll && swide[0] < 1.e29)
  {
    flat4_(swide, &iz, &m_Flatten);
  }

  ymin = 1.e30f;

  if(swide[0] > 1.e29 && swide[0] < 1.5e30) painter1.setPen(Qt::green); // horizontal line
  if(swide[0] > 1.4e30                    ) painter1.setPen(Qt::yellow);

  if(!m_bReplot)
  {
    m_j      =  0;
    int irow = -1;

    plotsave_(swide, &m_w, &m_h1, &irow);
  }

  for(int i = 0; i < iz; i++)
  {
    float const y = swide[i];
    if (y < ymin) ymin = y;
    int const y1 = std::clamp(static_cast<int>(10.0 * gain * y + m_plotZero), 0, 254);
    if (swide[i] < 1.e29) painter1.setPen(g_ColorTbl[y1]);
    painter1.drawPoint(i,m_j);
  }

  m_line++;

  float y2min =  1.e30f;
  float y2max = -1.e30f;

  for (int i = 0; i < iz; i++)
  {
    float const y = swide[i] - ymin;
    y2            = 0;
  
    if (m_bCurrent) y2 = gain2d * y + m_plot2dZero;            //Current

    if(bScroll)
    {
      float sum = 0.0f;
      int     k = j0 + m_binsPerPixel * i;
      for (int l = 0; l < m_binsPerPixel; l++)
      {
        sum += dec_data.savg[k++];
      }

      m_sum[i] = sum;
    }

    if (m_bCumulative ) y2  = gain2d * (m_sum[i] / m_binsPerPixel + m_plot2dZero);
    if (m_Flatten == 0) y2 += 15;                      //### could do better! ###

    if (m_bLinearAvg) //Linear Avg (yellow)
    {                                
      float sum = 0.0f;
      int     k = j0 + m_binsPerPixel * i;

      for (int l = 0; l < m_binsPerPixel; l++)
      {
        sum+=spectra_.syellow[k++];
      }

      y2 = gain2d * sum / m_binsPerPixel + m_plot2dZero;
    }

    if(m_bReference) //Reference (red)
    {
      float const df_ref = 12000.0f / 6912.0f;
      int   const k      = FreqfromX(i) / df_ref + 0.5;

      y2 = spectra_.ref[k] + m_plot2dZero;

    }

    if (i == iz - 1)
    {
      painter2D.drawPolyline(LineBuf, j);
      if(m_mode == "QRA64")
      {
        painter2D.setPen(Qt::red);
        painter2D.drawPolyline(LineBuf2, ktop);
      }
    }

    LineBuf[j].setX(i);
    LineBuf[j].setY(int(0.9 * m_h2 - y2 * m_h2 / 70.0));
    
    if (y2 < y2min) y2min = y2;
    if (y2 > y2max) y2max = y2;
    
    j++;
  }

  if(m_bReplot) return;

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
                      QString("%1    %2").arg(ts).arg(m_rxBand));
  }

  if(m_mode == "JT4" || m_mode== "QRA64")    //Mark freqs of JT4 single-tone msgs
  {
    int const y = 0.2 * m_h2;

    Font.setWeight(QFont::Bold);
    painter2D.setFont(Font);
    painter2D.setPen(Qt::yellow); 

    painter2D.drawText(XfromFreq(m_rxFreq      ) - 4, y, "T");
    painter2D.drawText(XfromFreq(m_rxFreq + 250) - 4, y, "M");
    painter2D.drawText(XfromFreq(m_rxFreq + 500) - 4, y, "R");
    painter2D.drawText(XfromFreq(m_rxFreq + 750) - 4, y, "73");
  }

  update();                                    //trigger a new paintEvent

  m_bScaleOK = true;
}

void CPlotter::drawDecodeLine(const QColor &color, int ia, int ib)
{
  int const x1 = XfromFreq(ia);
  int const x2 = XfromFreq(ib);

  QPainter painter1(&m_WaterfallPixmap);
  
  painter1.setPen(color);
  painter1.drawLine(qMin(x1, x2), 4, qMax(x1, x2), 4);
  painter1.drawLine(qMin(x1, x2), 0, qMin(x1, x2), 9);
  painter1.drawLine(qMax(x1, x2), 0, qMax(x1, x2), 9);
}

void CPlotter::drawHorizontalLine(const QColor &color, int x, int width)
{
  QPainter painter1(&m_WaterfallPixmap);

  painter1.setPen(color);
  painter1.drawLine(x, 0, width <= 0 ? m_w : x + width, 0);
}

void CPlotter::replot()
{
  float swide[m_w];

  m_bReplot = true;

  for (int irow = 0; irow < m_h1; irow++)
  {
    m_j = irow;
    plotsave_(swide, &m_w, &m_h1, &irow);
    draw(swide, false, false);
  }

  update();                                    //trigger a new paintEvent

  m_bReplot = false;
}

void CPlotter::DrawOverlay()                   //DrawOverlay()
{
  if (m_OverlayPixmap.isNull() ||
      m_WaterfallPixmap.isNull()) return;

  QPen penOrange(QColor(230, 126, 34),3);
  QPen penGray(QColor(149, 165, 166), 3);
  QPen penLightGreen(QColor(46, 204, 113), 3);
  QPen penLightYellow(QColor(241, 196, 15), 3);
  QPen penGreen(Qt::green, 3);

  QPainter painter(&m_OverlayPixmap);
  QLinearGradient gradient(0, 0, 0, m_h2);     //fill background with gradient
  gradient.setColorAt(1, Qt::black);
  gradient.setColorAt(0, Qt::darkBlue);
  painter.setBrush(gradient);
  painter.drawRect(0, 0, m_w, m_h2);
  painter.setBrush(Qt::SolidPattern);

  double const df = m_binsPerPixel * m_fftBinWidth;

  m_fSpan      = m_w * df;
  m_freqPerDiv = freqPerDiv(m_fSpan);

  float pixperdiv =           m_freqPerDiv / df;
  m_hdivs         = m_fSpan / m_freqPerDiv + 1.9999;

  float xx0 = float(m_startFreq) /float(m_freqPerDiv);
  xx0 = xx0 - int(xx0);
  int x0 = xx0 * pixperdiv + 0.5;
  for (int i = 1; i < m_hdivs; i++)  //draw vertical grids
  {
    if (int const x  = (int)((float)i * pixperdiv) - x0;
                  x >= 0 &&
                  x <= m_w)
    {
      painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
      painter.drawLine(x, 0, x , m_h2);
    }
  }

  pixperdiv = (float)m_h2 / (float)VERT_DIVS;
  painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
  for (int i = 1; i < VERT_DIVS; i++)  //draw horizontal grids
  {
    int const y = (int)( (float)i * pixperdiv );
    painter.drawLine(0, y, m_w, y);
  }

  QPainter painter0(&m_ScalePixmap);

  //create Font to use for scales
  QFont Font("Arial");
  Font.setPointSize(12);
  Font.setWeight(QFont::Normal);
  painter0.setFont(Font);
  painter0.setPen(Qt::black);

  if(m_binsPerPixel < 1) m_binsPerPixel=1;
  m_hdivs = m_w * df / m_freqPerDiv + 0.9999;

  m_ScalePixmap.fill(Qt::white);
  painter0.drawRect(0, 0, m_w, 30);
  MakeFrequencyStrs();

  //draw tick marks on upper scale
  pixperdiv = m_freqPerDiv / df;
  for (int i = 0; i < m_hdivs; i++)  //major ticks
  {
    int const x = (int)((m_xOffset+i)*pixperdiv );
    painter0.drawLine(x, 18, x, 30);
  }
  int const minor = m_freqPerDiv == 200 ? 4 : 5;
  for (int i = 1; i < minor * m_hdivs; i++)  //minor ticks
  {
    int const x = i * pixperdiv / minor;
    painter0.drawLine(x, 22, x, 30);
  }

  //draw frequency values
  for (int i = 0; i <= m_hdivs; i++)
  {
    if (int const x = (int)((m_xOffset + i) * pixperdiv - pixperdiv / 2);
              int(x + pixperdiv / 2) > 70)
    {
      painter0.drawText(QRect(x, 0, static_cast<int>(pixperdiv), 20),
                        Qt::AlignCenter,
                        m_HDivText[i]);
    }
  }

  painter0.setPen(penGreen);

  if (m_dialFreq > 10.13 &&
      m_dialFreq < 10.15 && !m_mode.startsWith(QLatin1StringView("WSPR")))
  {
    float const f1 = 1.0e6f * (10.1401 - m_dialFreq);
    float const f2 = f1 + 200.0f;
    if (int const x1  = XfromFreq(f1),
                  x2  = XfromFreq(f2);
                  x1 <= m_w &&
                  x2 >= 0)
    {
      painter0.setPen(penOrange);               //Mark WSPR sub-band orange
      painter0.drawLine(x1, 9, x2, 9);
    }
  }

  if (int const x1  = XfromFreq(0),
                x2  = XfromFreq(500);
                x1 <= m_w &&
                x2  > 0)
  {
    painter0.setPen(penGray);               //Mark bottom of sub-band
    painter0.drawLine(x1 + 1, 26, x2 - 2, 26);
    painter0.drawLine(x1 + 1, 28, x2 - 2, 28);
  }

  if (int const x1  = XfromFreq(3500),
                x2  = m_w;
                x1 <= m_w &&
                x2 >  0)
  {
    painter0.setPen(penGray);               //Mark top of sub-band
    painter0.drawLine(x1 + 1, 26, x2 - 2, 26);
    painter0.drawLine(x1 + 1, 28, x2 - 2, 28);
  }

#define JS8_DRAW_SUBBANDS 1
#if JS8_DRAW_SUBBANDS
  for (int i = 500; i <= 3000; i += 500)
  {
    if (int const x1  = XfromFreq(i),
                  x2  = XfromFreq(i + 500);
                  x1 <= m_w &&
                  x2 >  0)
    {
      switch(i) {
      case 500:
          painter0.setPen(penLightYellow);
          break;
      case 1000:
          painter0.setPen(penLightGreen);
          break;
      case 1500:
          painter0.setPen(penLightGreen);
          break;
      case 2000:
          painter0.setPen(penLightGreen);
          break;
      case 2500:
          painter0.setPen(penLightYellow);
          break;
      case 3000:
          painter0.setPen(penGray);
          break;
      }
      painter0.drawLine(x1 + 1, 26, x2 - 2, 26);
      painter0.drawLine(x1 + 1, 28, x2 - 2, 28);
    }
  }
  painter0.setPen(Qt::black);
  painter0.drawLine(0, 29, m_w, 29);
#endif

  // paint dials and filter overlays
  if (m_mode == "FT8")
  {
    int const fwidth = XfromFreq(m_rxFreq + bw(m_nSubMode)) - XfromFreq(m_rxFreq);

    DrawOverlayDial(fwidth);
    DrawOverlayHover(fwidth);
    DrawOverlayFilter();
  }
}

// Paint the dial overlay, showing the chunk of the frequency spectrum
// presently in use.

void
CPlotter::DrawOverlayDial(int const fwidth)
{
#if TEST_FOX_WAVE_GEN
  auto const bwValue = bw(m_nSubMode);
  int  const offset  = XfromFreq(m_rxFreq + bwValue + TEST_FOX_WAVE_GEN_OFFSET) - XfromFreq(m_rxFreq + bwValue + 4; // + 4 for the line padding
#endif

  QPainter p(&m_DialOverlayPixmap);

  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(rect(), Qt::transparent);
  p.setPen(Qt::red);
  p.drawLine(0,          30, 0,          m_h); // first slot, left line
  p.drawLine(fwidth + 1, 30, fwidth + 1, m_h); // first slot, right line

#if TEST_FOX_WAVE_GEN
  if (m_turbo)
  {
    for (int i = 1; i < TEST_FOX_WAVE_GEN_SLOTS; i++)
    {
      overPainter.drawLine(i*(fwidth + offset),              30, i*(fwidth + offset),              m_h); // n slot, left line
      overPainter.drawLine(i*(fwidth + offset) + fwidth + 2, 30, i*(fwidth + offset) + fwidth + 2, m_h); // n slot, right line
    }
  }
#endif

  p.setPen({Qt::red, 3});
  p.drawLine(0, 26, fwidth, 26); // first slot, top bar
  p.drawLine(0, 28, fwidth, 28); // first slot, top bar 2

#if TEST_FOX_WAVE_GEN
  if (m_turbo)
  {
    for (int i = 1; i < TEST_FOX_WAVE_GEN_SLOTS; i++)
    {
      overPainter.drawLine(i*(fwidth + offset) + 1, 26, i*(fwidth + offset) + fwidth + 1, 26); // n slot, top bar
      overPainter.drawLine(i*(fwidth + offset) + 1, 28, i*(fwidth + offset) + fwidth + 1, 28); // n slot, top bar 2
    }
  }
#endif
}

// Paint the hover overlay, showing the prospective chunk of frequency
// spectrum under the mouse.

void
CPlotter::DrawOverlayHover(int const fwidth)
{
  QPainter p(&m_HoverOverlayPixmap);

  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(rect(), Qt::transparent);
  p.setPen(Qt::white);
  p.drawLine(0,      30, 0,      m_h); // first slot, left line hover
  p.drawLine(fwidth, 30, fwidth, m_h); // first slot, right line hover

#if TEST_FOX_WAVE_GEN
  if (m_turbo)
  {
    for(int i = 1; i < TEST_FOX_WAVE_GEN_SLOTS; i++)
    {
      hoverPainter.drawLine(i*(fwidth + offset),              30, i*(fwidth + offset),              m_h); // n slot, left line
      hoverPainter.drawLine(i*(fwidth + offset) + fwidth + 2, 30, i*(fwidth + offset) + fwidth + 2, m_h); // n slot, right line
    }
  }
#endif

#if DRAW_FREQ_OVERLAY
  hoverPainter.setFont({"Arial"});
  hoverPainter.drawText(fwidth + 5, m_h, QString("%1").arg(FreqfromX(m_lastMouseX)));
#endif
}

// Paint the filter overlay pixmap, if the filter is enabled and has a width
// greater than zero. Note that we could be more clever here and ensure the
// filter is actually visible prior to painting, but what we're doing here
// is reasonably trivial, so probably not worth the effort.

void
CPlotter::DrawOverlayFilter()
{
  if (m_filterEnabled && m_filterWidth > 0)
  {
    QPainter p(&m_FilterOverlayPixmap);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);

    int const start = XfromFreq(m_filterCenter - m_filterWidth / 2);
    int const end   = XfromFreq(m_filterCenter + m_filterWidth / 2);

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

void CPlotter::MakeFrequencyStrs()                       //MakeFrequencyStrs
{
  int     f = (m_startFreq + m_freqPerDiv - 1) / m_freqPerDiv;
  f        *= m_freqPerDiv;
  m_xOffset = float(f - m_startFreq) / m_freqPerDiv;

  for (int i = 0; i <= m_hdivs; i++)
  {
    m_HDivText[i].setNum(f);
    f += m_freqPerDiv;
  }
}

int CPlotter::XfromFreq(float const f) const               //XfromFreq()
{
  return std::clamp(static_cast<int>(m_w * (f - m_startFreq) / m_fSpan + 0.5), 0, m_w);
}

float CPlotter::FreqfromX(int const x) const               //FreqfromX()
{
  return float(m_startFreq + x * m_binsPerPixel * m_fftBinWidth);
}

void CPlotter::setPlot2dGain(int const n)                 //setPlot2dGain
{
  m_plot2dGain = n;
  update();
}

void CPlotter::setStartFreq(int const f)                  //SetStartFreq()
{
  m_startFreq = f;
  resizeEvent(nullptr);
  update();
}

void CPlotter::UpdateOverlay()                           //UpdateOverlay
{
  DrawOverlay();
}

void CPlotter::setBinsPerPixel(int const n)             //setBinsPerPixel
{
  m_binsPerPixel = n;
  DrawOverlay();                         //Redraw scales and ticks
  update();                              //trigger a new paintEvent}
}

void CPlotter::setRxFreq(int const x)                   //setRxFreq
{
  m_rxFreq = x;         // x is freq in Hz
  DrawOverlay();
  update();
}

void CPlotter::leaveEvent(QEvent *)
{
    m_lastMouseX = -1;
}

void CPlotter::wheelEvent(QWheelEvent * event)
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

void CPlotter::mouseMoveEvent(QMouseEvent * event)
{
  auto const x = std::clamp(static_cast<int>(event->position().x()), 0, m_Size.width());

  m_lastMouseX = x;
#if DRAW_FREQ_OVERLAY
  DrawOverlay();
#endif
  update();
  event->ignore();

  QToolTip::showText(event->globalPosition().toPoint(),
                     QString::number(int(FreqfromX(x))));
}

void CPlotter::mouseReleaseEvent(QMouseEvent * event)
{
  if (Qt::LeftButton == event->button())
  {
    auto const x         = std::clamp(static_cast<int>(event->position().x()), 0, m_Size.width());
    bool const ctrl      = event->modifiers() & Qt::ControlModifier;
    bool const shift     = event->modifiers() & Qt::ShiftModifier;
    auto const newFreq   = int(FreqfromX(x)+0.5);
    int  const oldTxFreq = m_txFreq;
    int  const oldRxFreq = m_rxFreq;
    
    if      (ctrl)  { emit setFreq1(newFreq,   newFreq  ); }
    else if (shift) { emit setFreq1(oldRxFreq, newFreq  ); }
    else            { emit setFreq1(newFreq,   oldTxFreq); }

    emit freezeDecode1(ctrl ? 101 : 1);
  }
  else
  {
    event->ignore();           // let parent handle
  }
}

void CPlotter::mouseDoubleClickEvent(QMouseEvent * event)
{
  if (Qt::LeftButton == event->button())
  {
    emit freezeDecode1(event->modifiers() & Qt::ControlModifier ? 102 : 2);
  }
  else
  {
    event->ignore();           // let parent handle
  }
}

void CPlotter::setNsps(int const ntrperiod, int const nsps)                    //setNsps
{
  m_TRperiod    = ntrperiod;
  m_nsps        = nsps;
  m_fftBinWidth = fftBinWidth(nsps);

  DrawOverlay(); //Redraw scales and ticks
  update();      //trigger a new paintEvent}
}

void CPlotter::setTxFreq(int const n)                            //setTxFreq
{
  m_txFreq = n;
  DrawOverlay();
  update();
}

void CPlotter::setDialFreq(double const d)
{
  m_dialFreq = d;
  DrawOverlay();
  update();
}

void CPlotter::setRxBand(QString const & band)
{
  m_rxBand = band;
  DrawOverlay();
  update();
}

void CPlotter::setTurbo(bool const turbo)
{
  m_turbo = turbo;
  DrawOverlay();
  update();
}

void CPlotter::setFilterCenter(int const center)
{
  m_filterCenter = center;
  DrawOverlay();
  update();
}

void CPlotter::setFilterWidth(int const width)
{
  m_filterWidth = width;
  DrawOverlay();
  update();
}

void CPlotter::setFilterEnabled(bool const enabled)
{
  m_filterEnabled = enabled;
  DrawOverlay();
  update();
}

void CPlotter::setFilterOpacity(int const alpha)
{
  m_filterOpacity = alpha;
  DrawOverlay();
  update();
}

void
CPlotter::setRxRange(int const fMin)
{
  m_fMin = fMin;
  DrawOverlay();
  update();
}

void
CPlotter::setMode(QString const & mode)
{
  m_mode = mode;
  DrawOverlay();
  update();
}

void
CPlotter::setModeTx(QString const & modeTx)
{
  m_modeTx = modeTx;
  DrawOverlay();
  update();
}

void
CPlotter::setSubMode(int const nSubMode)
{
  m_nSubMode = nSubMode;
  DrawOverlay();
  update();
}

void
CPlotter::setTol(int const n)
{
  m_tol = n; // XXX this is never referenced
  DrawOverlay();
  update();
}

void CPlotter::setFlatten(bool const b1, bool const b2)
{
          m_Flatten = 0;
  if (b1) m_Flatten = 1;
  if (b2) m_Flatten = 2;
}

void CPlotter::SetPercent2DScreen(int percent)
{
  m_Percent2DScreen=percent;
  resizeEvent(nullptr);
  update();
}
