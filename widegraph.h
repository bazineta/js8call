// -*- Mode: C++ -*-
#ifndef WIDEGRAPH_H
#define WIDEGRAPH_H

#include <array>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QMutex>
#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include "commons.h"
#include "WF.hpp"

namespace Ui {
  class WideGraph;
}

class Configuration;
class QSettings;

class FocusEater : public QObject
{
   Q_OBJECT
public:
  explicit FocusEater(QObject * parent = nullptr)
  : QObject(parent)
  {}

  virtual bool
  eventFilter(QObject * object,
              QEvent  * event) override
   {
    Q_UNUSED(object)
    if      (event->type() == QEvent::FocusIn)  emit focused(object);
    else if (event->type() == QEvent::FocusOut) emit blurred(object);
      return false;
   }

signals:
  void focused(QObject *);
  void blurred(QObject *);
};

class WideGraph : public QWidget
{
  Q_OBJECT

public:
  explicit WideGraph(QSettings *, QWidget * = nullptr);
  ~WideGraph ();

  void   dataSink2(float[], float);
  void   setRxFreq(int n);
  int    rxFreq() const;
  int    centerFreq() const;
  int    nStartFreq() const;
  int    filterMinimum() const;
  int    filterMaximum() const;
  bool   filterEnabled() const;
  void   setFilterCenter(int);
  void   setFilter(int, int);
  void   setFilterMinimumBandwidth(int);
  void   setFilterEnabled(bool);
  void   setFilterOpacityPercent(int);
  int    fSpan() const;
  void   saveSettings();
  void   setPeriod(int);
  void   setTxFreq(int);
  void   setSubMode(int);
  int    smoothYellow() const;
  void   setBand (QString const &);
  void   drawDecodeLine(const QColor &color, int ia, int ib);
  void   drawHorizontalLine(const QColor &color, int x, int width);
  bool   shouldDisplayDecodeAttempts() const;
  bool   shouldAutoSyncSubmode(int) const;
  bool   isAutoSyncEnabled() const;
  QVector<QColor> const& colors() const;

signals:
  void f11f12(int n);
  void setXIT2(int n);
  void setFreq3(int rxFreq, int txFreq);
  void qsy(int hzDelta);
  void drifted(int prev, int cur);

public slots:
  void setFreq2(int rxFreq, int txFreq);
  void setDialFreq(double);
  void setTimeControlsVisible(bool);
  bool timeControlsVisible() const;
  void setControlsVisible(bool);
  bool controlsVisible() const;
  void setDrift(int);
  int  drift() const;
  void setPaused(bool paused){ m_paused = paused; }
  void notifyDriftedSignalsDecoded(int signalsDecoded);

protected:
  void keyPressEvent (QKeyEvent *e) override;
  void closeEvent (QCloseEvent *) override;

private slots:
  void draw();
  void drawSwide();

  void on_qsyPushButton_clicked();
  void on_offsetSpinBox_valueChanged(int n);
  void on_waterfallAvgSpinBox_valueChanged(int arg1);
  void on_bppSpinBox_valueChanged(int arg1);
  void on_spec2dComboBox_currentIndexChanged(int);
  void on_fStartSpinBox_valueChanged(int n);
  void on_paletteComboBox_activated(int);
  void on_cbFlatten_toggled(bool b);
  void on_cbControls_toggled(bool b);
  void on_adjust_palette_push_button_clicked (bool);
  void on_gainSlider_valueChanged(int value);
  void on_zeroSlider_valueChanged(int value);
  void on_gain2dSlider_valueChanged(int value);
  void on_zero2dSlider_valueChanged(int value);
  void on_smoSpinBox_valueChanged(int n);  
  void on_sbPercent2dPlot_valueChanged(int n);
  void on_filterMinSpinBox_valueChanged(int n);
  void on_filterMaxSpinBox_valueChanged(int n);
  void on_filterCenterSpinBox_valueChanged(int n);
  void on_filterWidthSpinBox_valueChanged(int n);
  void on_filterCenterSyncButton_clicked();
  void on_filterCheckBox_toggled(bool b);
  void on_filterOpacitySpinBox_valueChanged(int n);

  void on_autoDriftButton_toggled(bool checked);
  void on_driftSpinBox_valueChanged(int n);
  void on_driftSyncButton_clicked();
  void on_driftSyncEndButton_clicked();
  void on_driftSyncMinuteButton_clicked();
  void on_driftSyncResetButton_clicked();


private:

  static constexpr qint32 MaxScreenSize = 2048;

  using SWide = std::array<float, MaxScreenSize>;
  using SPlot = std::array<float, NSMAX>;

  void readPalette ();
  void replot();

  QScopedPointer<Ui::WideGraph> ui;

  QSettings * m_settings;
  QDir        m_palettes_path;
  WF::Palette m_userPalette;
  SWide       m_swide;
  SPlot       m_splot;

  bool m_filterEnabled;

  quint64 m_lastLoop = 0;
  
  int m_filterMinWidth;
  int m_filterMinimum;
  int m_filterMaximum;
  int m_filterCenter;
  int m_waterfallAvg;
  int m_TRperiod;
  int m_ntr0;
  int m_nsmo;
  int m_percent2DScreen;
  int m_jz = MaxScreenSize;
  int m_n;

  bool   m_paused;
  bool   m_flatten;
  bool   m_autoSyncConnected = false;

  QTimer m_autoSyncTimer;
  int    m_autoSyncTimeLeft;
  int    m_autoSyncDecodesLeft;
  int    m_lastSecondInPeriod = 0;

  QTimer m_drawTimer;
  QMutex m_drawLock;

  QString m_waterfallPalette;
};

#endif // WIDEGRAPH_H
