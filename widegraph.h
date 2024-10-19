// -*- Mode: C++ -*-
#ifndef WIDEGRAPH_H
#define WIDEGRAPH_H


#include <array>
#include <random>
#include <iterator>
#include <iostream>

#include <QDialog>
#include <QEvent>
#include <QScopedPointer>
#include <QDir>
#include <QHash>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QVariant>
#include "commons.h"
#include "WF.hpp"

namespace Ui {
  class WideGraph;
}

class QSettings;
class Configuration;

class FocusEater : public QObject
{
   Q_OBJECT
public:
   explicit FocusEater(QObject* parent = nullptr) : QObject(parent)
   {
   }

   virtual bool eventFilter(QObject *obj, QEvent *event) override
   {
      Q_UNUSED(obj)
      if (event->type() == QEvent::FocusIn){
         emit focused(obj);
      }
      else if (event->type() == QEvent::FocusOut){
         emit blurred(obj);
      }

      return false;
   }

signals:
   void focused(QObject *obj);
   void blurred(QObject *obj);
};

class WideGraph : public QDialog
{
  Q_OBJECT

public:
  explicit WideGraph(QSettings *, QWidget *parent = nullptr);
  ~WideGraph ();

  void   dataSink2(float s[], float df3, int ihsym);
  void   setRxFreq(int n);
  int    rxFreq();
  int    centerFreq();
  int    nStartFreq();
  int    Fmin();
  int    Fmax();
  int    filterMinimum();
  int    filterMaximum();
  bool   filterEnabled();
  void   setFilterCenter(int n);
  void   setFilter(int a, int b);
  void   setFilterMinimumBandwidth(int width);
  void   setFilterEnabled(bool enabled);
  void   setFilterOpacityPercent(int n);
  int    fSpan();
  void   saveSettings();
  void   setFsample(int n);
  void   setPeriod(int ntrperiod, int nsps);
  void   setTxFreq(int n);
  void   setMode(QString mode);
  void   setSubMode(int n);
  bool   flatten();
  bool   useRef();
  int    smoothYellow();
  void   setRxBand (QString const& band);
  void   setWSPRtransmitted();
  void   drawDecodeLine(const QColor &color, int ia, int ib);
  void   drawHorizontalLine(const QColor &color, int x, int width);
  void   setRedFile(QString fRed);
  bool   shouldDisplayDecodeAttempts();
  bool   shouldAutoSyncSubmode(int submode);
  bool   isAutoSyncEnabled();
  QVector<QColor> const& colors();

signals:
  void f11f12(int n);
  void setXIT2(int n);
  void setFreq3(int rxFreq, int txFreq);
  void qsy(int hzDelta);
  void drifted(int prev, int cur);

public slots:
  void setFreq2(int rxFreq, int txFreq);
  void setDialFreq(double d);
  void setTimeControlsVisible(bool visible);
  bool timeControlsVisible();
  void setControlsVisible(bool visible);
  bool controlsVisible();
  void setDrift(int n);
  int drift();
  void setQSYEnabled(bool enabled);
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
  void on_cbRef_toggled(bool b);
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
  void setRxRange ();
  void replot();

  QScopedPointer<Ui::WideGraph> ui;

  QSettings              * m_settings;
  QDir                     m_palettes_path;
  WF::Palette              m_userPalette;
  QHash<QString, QVariant> m_fMinPerBand;
  SWide                    m_swide;
  SPlot                    m_splot;

  bool m_filterEnabled;

  quint64 m_lastLoop = 0;
  
  qint32 m_filterMinWidth;
  qint32 m_filterMinimum;
  qint32 m_filterMaximum;
  qint32 m_filterCenter;
  qint32 m_waterfallAvg;
  qint32 m_TRperiod;
  qint32 m_nsps;
  qint32 m_ntr0;
  qint32 m_fMax;
  qint32 m_nsmo;
  qint32 m_Percent2DScreen;
  qint32 m_jz = MaxScreenSize;
  qint32 m_n;

  bool   m_paused;
  bool   m_bFlatten;
  bool   m_bRef;
  bool   m_bHaveTransmitted;    //Set true at end of a WSPR transmission
  bool   m_autoSyncConnected = false;

  QTimer m_autoSyncTimer;
  int    m_autoSyncTimeLeft;
  int    m_autoSyncDecodesLeft;
  int    m_lastSecondInPeriod = 0;

  QTimer m_drawTimer;
  QMutex m_drawLock;

  QString m_rxBand;
  QString m_waterfallPalette;  

  std::default_random_engine m_gen;
  std::normal_distribution<double> m_dist;
};

#endif // WIDEGRAPH_H
