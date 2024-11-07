// -*- Mode: C++ -*-
///////////////////////////////////////////////////////////////////////////
// Some code in this file and accompanying files is based on work by
// Moe Wheatley, AE4Y, released under the "Simplified BSD License".
// For more details see the accompanying file LICENSE_WHEATLEY.TXT
///////////////////////////////////////////////////////////////////////////

#ifndef PLOTTER_H
#define PLOTTER_H

#include <array>
#include <QColor>
#include <QFrame>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QVector>
#include "WF.hpp"

class CPlotter : public QFrame
{
  Q_OBJECT

public:

  using Colors   = QVector<QColor>;
  using Spectrum = WF::Spectrum;

  explicit CPlotter(QWidget *parent = nullptr);
  ~CPlotter();

  // Sizing

  QSize minimumSizeHint() const override;
  QSize sizeHint()        const override;

  // Inline accessors

  int            binsPerPixel() const { return m_binsPerPixel; }
  Colors const & colors()       const { return m_colors;       }
  int            freq()         const { return m_freq;         }
  int            plot2dGain()   const { return m_plot2dGain;   }
  int            plot2dZero()   const { return m_plot2dZero;   }
  int            plotGain()     const { return m_plotGain;     }
  int            plotWidth()    const { return m_w;            }
  int            plotZero()     const { return m_plotZero;     }
  bool           scaleOK()      const { return m_scaleOK;      }
  Spectrum       spectrum()     const { return m_spectrum;     }
  int            startFreq()    const { return m_startFreq;    }

  int
  frequencyAt(int const x) const
  {
    return static_cast<int>(freqFromX(x));
  }

  // Inline manipulators

  void setColors      (Colors   const & colors      ) { m_colors       = colors;       }
  void setFlatten     (bool     const   flatten     ) { m_flatten      = flatten;      }
  void setPlot2dZero  (int      const   plot2dZero  ) { m_plot2dZero   = plot2dZero;   }
  void setPlotGain    (int      const   plotGain    ) { m_plotGain     = plotGain;     }
  void setPlotZero    (int      const   plotZero    ) { m_plotZero     = plotZero;     }
  void setSpectrum    (Spectrum const   spectrum    ) { m_spectrum     = spectrum;     }
  void setWaterfallAvg(int      const   waterfallAvg) { m_waterfallAvg = waterfallAvg; }

  // Manipulators

  void draw(float[], bool);
  void drawDecodeLine    (const QColor &, int, int);
  void drawHorizontalLine(const QColor &, int, int);
  void replot();
  void setBand(QString const &);
  void setBinsPerPixel(int);
  void setDialFreq(float);
  void setFilterCenter(int);
  void setFilterEnabled(bool);
  void setFilterWidth(int);
  void setFilterOpacity(int);
  void setFreq(int);
  void setPercent2DScreen(int);
  void setPeriod(int);
  void setPlot2dGain(int);
  void setStartFreq(int);
  void setSubMode(int nSubMode);

signals:

  void changeFreq(int);

protected:

  // Event Handlers

  void paintEvent       (QPaintEvent  *) override;
  void resizeEvent      (QResizeEvent *) override;
  void leaveEvent       (QEvent       *) override;
  void wheelEvent       (QWheelEvent  *) override;
  void mouseMoveEvent   (QMouseEvent  *) override;
  void mouseReleaseEvent(QMouseEvent  *) override;

private:

  static constexpr std::size_t MaxScreenSize = 2048;

  // Accessors

  bool  in30MBand()        const;
  int   xFromFreq(float f) const;
  float freqFromX(int   x) const;

  // Manipulators

  void drawSpectrum(int);
  void drawOverlay();
  void drawOverlayScale(int, float, std::size_t);
  void drawOverlaySubmode();
  void drawOverlayDial(QRect const &);
  void drawOverlayHover(QRect const &);
  void drawOverlayFilter();

  std::array<float,  MaxScreenSize> m_sum    = {};
  std::array<QPoint, MaxScreenSize> m_points = {};

  Colors   m_colors;
  Spectrum m_spectrum = Spectrum::Current;

  QPixmap m_FilterOverlayPixmap;
  QPixmap m_DialOverlayPixmap;
  QPixmap m_HoverOverlayPixmap;
  QPixmap m_WaterfallPixmap;
  QPixmap m_SpectrumPixmap;
  QPixmap m_ScalePixmap;
  QPixmap m_OverlayPixmap;

  QSize   m_size;
  QString m_band;

  double m_freqPerPixel;
  float  m_dialFreq         = 0.0f;
  int    m_nSubMode         =  0;
  int    m_filterCenter     =  0;
  int    m_filterWidth      =  0;
  int    m_filterOpacity    =  127;
  int    m_percent2DScreen  =  0;
  int    m_percent2DScreen0 =  0;
  int    m_plotZero         =  0;
  int    m_plotGain         =  0;
  int    m_plot2dGain       =  0;
  int    m_plot2dZero       =  0;
  int    m_binsPerPixel     =  2;
  int    m_waterfallAvg     =  1;
  int    m_lastMouseX       = -1;
  int    m_line             =  0;
  int    m_startFreq        =  0;
  int    m_freq             =  0;
  int    m_w                =  0;
  int    m_h                =  0;
  int    m_h1               =  0;
  int    m_h2               =  0;
  int    m_j                =  0;
  int    m_TRperiod         =  15;
  bool   m_filterEnabled    = false;
  bool   m_paintEventBusy   = false;
  bool   m_scaleOK          = false;
  bool   m_replot           = false;
  bool   m_flatten          = false;
};

#endif // PLOTTER_H
