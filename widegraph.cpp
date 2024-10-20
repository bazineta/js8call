#include "widegraph.h"
#include <algorithm>
#include <QMenu>
#include <QMutexLocker>
#include <QSettings>
#include <QSignalBlocker>
#include "ui_widegraph.h"
#include "Configuration.hpp"
#include "DriftingDateTime.h"
#include "MessageBox.hpp"
#include "SettingsGroup.hpp"
#include "keyeater.h"
#include "varicode.h"

#include "moc_widegraph.cpp"

namespace
{
  auto const user_defined = QObject::tr ("User Defined");

  // Set the spinbox to the value, ensuring that signals are
  // blocked during the set operation and restoring the prior
  // blocked state afterward.

  void
  setValueBlocked(int const  value,
                  QSpinBox * block)
  {
    QSignalBlocker blocker(block);
    block->setValue(value);
  };

  // Set the checkbox to the value, ensuring that signals are
  // blocked during the set operation and restoring the prior
  // blocked state afterward.

  void
  setValueBlocked(bool  const value,
                  QCheckBox * block)
  {
    QSignalBlocker blocker(block);
    block->setChecked(value);
  };
}

WideGraph::WideGraph(QSettings * settings,
                     QWidget   * parent)
: QWidget         {parent},
  ui(new Ui::WideGraph),
  m_settings (settings),
  m_palettes_path {":/Palettes"},
  m_filterEnabled {false},
  m_filterMinWidth {0},
  m_filterMinimum {0},
  m_filterMaximum {5000},
  m_ntr0 {0},
  m_n {0}
{
  ui->setupUi(this);

  setMaximumHeight (880);

  ui->splitter->setChildrenCollapsible(false);
  ui->splitter->setCollapsible(ui->splitter->indexOf(ui->controls_widget), false);
  ui->splitter->updateGeometry();

  auto focusEater = new FocusEater(this);
  connect(focusEater, &FocusEater::blurred, this, [this](QObject * /*obj*/){
      setFilter(filterMinimum(), filterMaximum());
  });
  ui->filterMinSpinBox->installEventFilter(focusEater);
  ui->filterMaxSpinBox->installEventFilter(focusEater);

  auto filterEscapeEater = new KeyPressEater();
  connect(filterEscapeEater, &KeyPressEater::keyPressed, this, [this](QObject */*obj*/, QKeyEvent *e, bool *pProcessed){
      if(e->key() != Qt::Key_Escape){
          return;
      }
      setFilter(0, 5000);
      if(pProcessed) *pProcessed=true;
  });
  ui->filterMinSpinBox->installEventFilter(filterEscapeEater);
  ui->filterMaxSpinBox->installEventFilter(filterEscapeEater);

  ui->widePlot->setCursor(Qt::CrossCursor);
  ui->widePlot->setMaximumWidth(MaxScreenSize);
  ui->widePlot->setMaximumHeight(800);

  ui->widePlot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->widePlot, &CPlotter::customContextMenuRequested, this, [this](const QPoint &pos){
      auto menu = new QMenu(this);

      auto const f = ui->widePlot->frequencyAt(pos.x());

      auto offsetAction = menu->addAction(QString("Set &Offset to %1 Hz").arg(f));
      connect(offsetAction, &QAction::triggered, this, [this, f](){
        ui->offsetSpinBox->setValue(f);
      });

      menu->addSeparator();

      if(m_filterEnabled){
          auto disableAction = menu->addAction(QString("&Disable Filter"));
          connect(disableAction, &QAction::triggered, this, [this](){
            ui->filterCheckBox->setChecked(false);
          });
      }

      auto centerAction = menu->addAction(QString("Set Filter &Center to %1 Hz").arg(f));
      connect(centerAction, &QAction::triggered, this, [this, f](){
        ui->filterCenterSpinBox->setValue(f);
        ui->filterCheckBox->setChecked(true);
      });

      auto widthMenu = menu->addMenu("Set Filter &Width to...");
      auto widths = QList<int>{ 25, 50, 75, 100, 250, 500, 750, 1000, 1500, 2000 };
      foreach(auto width, widths){
        if(width < m_filterMinWidth){ continue; }
        auto widthAction = widthMenu->addAction(QString("%1 Hz").arg(width));
        connect(widthAction, &QAction::triggered, this, [this, width](){
            ui->filterWidthSpinBox->setValue(width);
            ui->filterCheckBox->setChecked(true);
        });
      }

      auto minAction = menu->addAction(QString("Set Filter &Minimum to %1 Hz").arg(f));
      connect(minAction, &QAction::triggered, this, [this, f](){
        ui->filterMinSpinBox->setValue(f);
        ui->filterCheckBox->setChecked(true);
      });

      auto maxAction = menu->addAction(QString("Set Filter Ma&ximum to %1 Hz").arg(f));
      connect(maxAction, &QAction::triggered, this, [this, f](){
        ui->filterMaxSpinBox->setValue(f);
        ui->filterCheckBox->setChecked(true);
      });

      menu->popup(ui->widePlot->mapToGlobal(pos));
  });

  connect(ui->widePlot, SIGNAL(setFreq1(int,int)),this,
          SLOT(setFreq2(int,int)));

  {

    //Restore user's settings
    SettingsGroup g {m_settings, "WideGraph"};
    restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
    ui->widePlot->setPlotZero(m_settings->value("PlotZero", 0).toInt());
    ui->widePlot->setPlotGain(m_settings->value("PlotGain", 0).toInt());
    ui->widePlot->setPlot2dGain(m_settings->value("Plot2dGain", 0).toInt());
    ui->widePlot->setPlot2dZero(m_settings->value("Plot2dZero", 0).toInt());
    ui->zeroSlider->setValue(ui->widePlot->plotZero());
    ui->gainSlider->setValue(ui->widePlot->plotGain());
    ui->gain2dSlider->setValue(ui->widePlot->plot2dGain());
    ui->zero2dSlider->setValue(ui->widePlot->plot2dZero());
    int n = m_settings->value("BinsPerPixel",2).toInt();
    m_bFlatten=m_settings->value("Flatten",true).toBool();
    ui->cbFlatten->setChecked(m_bFlatten);
    ui->widePlot->setFlatten(m_bFlatten);
    ui->bppSpinBox->setValue(n);
    m_nsmo=m_settings->value("SmoothYellow",1).toInt();
    ui->smoSpinBox->setValue(m_nsmo);
    m_Percent2DScreen=m_settings->value("Percent2D", 0).toInt();
    m_waterfallAvg = m_settings->value("WaterfallAvg", 1).toInt();
    ui->waterfallAvgSpinBox->setValue(m_waterfallAvg);
    ui->widePlot->setWaterfallAvg(m_waterfallAvg);
    ui->widePlot->setSpectrum(m_settings->value("WaterfallSpectrum", QVariant::fromValue(WF::Spectrum::Cumulative)).value<WF::Spectrum>());
    if(ui->widePlot->spectrum() == WF::Spectrum::Current)    ui->spec2dComboBox->setCurrentIndex(0);
    if(ui->widePlot->spectrum() == WF::Spectrum::Cumulative) ui->spec2dComboBox->setCurrentIndex(1);
    if(ui->widePlot->spectrum() == WF::Spectrum::LinearAvg)  ui->spec2dComboBox->setCurrentIndex(2);
    int nbpp=m_settings->value("BinsPerPixel", 2).toInt();
    ui->widePlot->setBinsPerPixel(nbpp);
    ui->sbPercent2dPlot->setValue(m_Percent2DScreen);
    ui->widePlot->setPercent2DScreen(m_Percent2DScreen);
    ui->widePlot->setStartFreq(m_settings->value("StartFreq", 500).toInt());
    ui->centerSpinBox->setValue(m_settings->value("CenterOffset", 1500).toInt());
    ui->fStartSpinBox->setValue(ui->widePlot->startFreq());
    m_waterfallPalette=m_settings->value("WaterfallPalette","Default").toString();
    m_userPalette = WF::Palette {m_settings->value("UserPalette").value<WF::Palette::Colours> ()};
    ui->controls_widget->setVisible(!m_settings->value("HideControls", false).toBool());
    ui->cbControls->setChecked(!m_settings->value("HideControls", false).toBool());
    ui->fpsSpinBox->setValue(m_settings->value ("WaterfallFPS", 4).toInt());
    ui->decodeAttemptCheckBox->setChecked(m_settings->value("DisplayDecodeAttempts", false).toBool());
    ui->autoDriftAutoStopCheckBox->setChecked(m_settings->value ("StopAutoSyncOnDecode", true).toBool());
    ui->autoDriftStopSpinBox->setValue(m_settings->value ("StopAutoSyncAfter", 1).toInt());

    auto splitState = m_settings->value("SplitState").toByteArray();
    if(!splitState.isEmpty()){
        ui->splitter->restoreState(splitState);
    }

    setFilter(m_settings->value("FilterMinimum", 500).toInt(), m_settings->value("FilterMaximum", 2500).toInt());
    setFilterOpacityPercent(m_settings->value("FilterOpacityPercent", 50).toInt());
    setFilterEnabled(m_settings->value("FilterEnabled", false).toBool());
  }

  int index=0;
  for (QString const& file:
         m_palettes_path.entryList(QDir::NoDotAndDotDot |
                                   QDir::System | QDir::Hidden |
                                   QDir::AllDirs | QDir::Files,
                                   QDir::DirsFirst)) {
    QString t=file.mid(0,file.length()-4);
    ui->paletteComboBox->addItem(t);
    if(t==m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
    index++;
  }
  ui->paletteComboBox->addItem (user_defined);
  if (user_defined == m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
  readPalette ();

  connect(&m_drawTimer, &QTimer::timeout, this, &WideGraph::draw);
  m_drawTimer.setSingleShot(true);
  m_drawTimer.start(100);   //### Don't change the 100 ms! ###
}

WideGraph::~WideGraph()
{}

void
WideGraph::closeEvent (QCloseEvent * event)
{
  saveSettings();
  QWidget::closeEvent(event);
}

void WideGraph::saveSettings()                                           //saveSettings
{
  SettingsGroup g {m_settings, "WideGraph"};
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("PlotZero", ui->widePlot->plotZero());
  m_settings->setValue ("PlotGain", ui->widePlot->plotGain());
  m_settings->setValue ("Plot2dGain", ui->widePlot->plot2dGain());
  m_settings->setValue ("Plot2dZero", ui->widePlot->plot2dZero());
  m_settings->setValue ("BinsPerPixel", ui->bppSpinBox->value ());
  m_settings->setValue ("SmoothYellow", ui->smoSpinBox->value ());
  m_settings->setValue ("Percent2D",m_Percent2DScreen);
  m_settings->setValue ("WaterfallAvg", ui->waterfallAvgSpinBox->value ());
  m_settings->setValue ("WaterfallSpectrum", QVariant::fromValue(ui->widePlot->spectrum()));
  m_settings->setValue ("BinsPerPixel", ui->widePlot->binsPerPixel ());
  m_settings->setValue ("StartFreq", ui->widePlot->startFreq ());
  m_settings->setValue ("WaterfallPalette", m_waterfallPalette);
  m_settings->setValue ("UserPalette", QVariant::fromValue (m_userPalette.colours ()));
  m_settings->setValue ("Flatten",m_bFlatten);
  m_settings->setValue ("HideControls", ui->controls_widget->isHidden ());
  m_settings->setValue ("CenterOffset", ui->centerSpinBox->value());
  m_settings->setValue ("FilterMinimum", m_filterMinimum);
  m_settings->setValue ("FilterMaximum", m_filterMaximum);
  m_settings->setValue ("FilterEnabled", m_filterEnabled);
  m_settings->setValue ("FilterOpacityPercent", ui->filterOpacitySpinBox->value());
  m_settings->setValue ("SplitState", ui->splitter->saveState());
  m_settings->setValue ("WaterfallFPS", ui->fpsSpinBox->value());
  m_settings->setValue ("DisplayDecodeAttempts", ui->decodeAttemptCheckBox->isChecked());
  m_settings->setValue ("StopAutoSyncOnDecode", ui->autoDriftAutoStopCheckBox->isChecked());
  m_settings->setValue ("StopAutoSyncAfter", ui->autoDriftStopSpinBox->value());
}

bool
WideGraph::shouldDisplayDecodeAttempts() const
{
  return ui->decodeAttemptCheckBox->isChecked();
}

bool
WideGraph::isAutoSyncEnabled() const
{
  // enabled if we're auto drifting
  // and we are not auto stopping
  // or if we are auto stopping,
  // we have auto sync decodes left
  return ui->autoDriftButton->isChecked() && (
      !ui->autoDriftAutoStopCheckBox->isChecked() ||
      m_autoSyncDecodesLeft > 0
  );
}

bool
WideGraph::shouldAutoSyncSubmode(int submode) const
{
  return isAutoSyncEnabled() && (
          submode == Varicode::JS8CallSlow
      || submode == Varicode::JS8CallNormal
  //  || submode == Varicode::JS8CallFast
  //  || submode == Varicode::JS8CallTurbo
  //  || submode == Varicode::JS8CallUltra
  );
}

void WideGraph::notifyDriftedSignalsDecoded(int signalsDecoded){
    //qDebug() << "decoded" << signalsDecoded << "with" << m_autoSyncDecodesLeft << "left";

    m_autoSyncDecodesLeft -= signalsDecoded;

    if(ui->autoDriftAutoStopCheckBox->isChecked() && m_autoSyncDecodesLeft <= 0){
        ui->autoDriftButton->setChecked(false);
    }
}

void WideGraph::on_autoDriftButton_toggled(bool checked){
    if(!m_autoSyncConnected){
        connect(&m_autoSyncTimer, &QTimer::timeout, this, [this](){
            // if auto drift isn't checked, don't worry about this...
            if(!ui->autoDriftButton->isChecked()){
                return;
            }

            // uncheck after timeout
            if(m_autoSyncTimeLeft == 0){
                ui->autoDriftButton->setChecked(false);
                return;
            }

            // set new text and decrement timeleft
            auto text = ui->autoDriftButton->text();
            auto newText = QString("%1 (%2)").arg(text.left(text.indexOf("(")).trimmed()).arg(m_autoSyncTimeLeft--);
            ui->autoDriftButton->setText(newText);
        });
        m_autoSyncConnected = true;
    }

    // if in the future we want to auto sync timeout after a time period
    bool autoSyncTimeout = false;

    auto text = ui->autoDriftButton->text();

    if(autoSyncTimeout){
        if(checked){
            m_autoSyncTimeLeft = 120;
            m_autoSyncTimer.setInterval(1000);
            m_autoSyncTimer.start();
            ui->autoDriftButton->setText(QString("%1 (%2)").arg(text.replace("Start", "Stop")).arg(m_autoSyncTimeLeft--));
        } else {
            m_autoSyncTimeLeft = 0;
            m_autoSyncTimer.stop();
            ui->autoDriftButton->setText(text.left(text.indexOf("(")).trimmed().replace("Stop", "Start"));
        }
        return;
    } else {
        if(checked){
            m_autoSyncDecodesLeft = ui->autoDriftStopSpinBox->value();
            ui->autoDriftButton->setText(text.left(text.indexOf("(")).trimmed().replace("Start", "Stop"));
            ui->autoDriftStopSpinBox->setEnabled(false);
        } else {
            m_autoSyncDecodesLeft = 0;
            ui->autoDriftButton->setText(text.left(text.indexOf("(")).trimmed().replace("Stop", "Start"));
            ui->autoDriftStopSpinBox->setEnabled(true);
        }
    }
}

void
WideGraph::drawDecodeLine(QColor const & color,
                          int    const   ia,
                          int    const   ib)
{
  ui->widePlot->drawDecodeLine(color, ia, ib);
}

void
WideGraph::drawHorizontalLine(QColor const & color,
                              int    const    x,
                              int    const    width)
{
    ui->widePlot->drawHorizontalLine(color, x, width);
}

void
WideGraph::dataSink2(float s[],
                     float df3)
{
  QMutexLocker lock(&m_drawLock);

  int nbpp = ui->widePlot->binsPerPixel();

  //Average spectra over specified number, m_waterfallAvg
  if (m_n == 0)
  {
    std::copy_n(s, m_splot.size(), m_splot.begin());
  }
  else
  {
    std::transform(m_splot.begin(),
                   m_splot.end(),
                   s,
                   m_splot.begin(),
                   std::plus<>{});
  }
  m_n++;

  if (m_n<m_waterfallAvg) {
      return;
  }

  for (auto & item : m_splot) item /= m_n; //Normalize the average

  m_n=0;
  int i=int(ui->widePlot->startFreq()/df3 + 0.5);
  int jz=5000.0/(nbpp*df3);
  if (jz > MaxScreenSize) jz = MaxScreenSize;
  m_jz=jz;
  for (int j=0; j<jz; j++) {
    float ss=0.0;
    float smax=0;
    for (int k=0; k<nbpp; k++) {
      float sp=m_splot[i++];
      ss += sp;
      smax=qMax(smax,sp);
    }

    // swide[j]=nbpp*smax;
    m_swide[j]=nbpp*ss;
  }

  // Draw the tr cycle horizontal lines if needed.
  qint64 const ms  = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
  int    const ntr = (ms/1000) % m_TRperiod;
  if (ntr < m_ntr0)
  {
    m_splot.fill(1.0e30f);
  }
  m_ntr0=ntr;
}

void
WideGraph::draw()
{
  quint64 const fps      = qMax(1, qMin(ui->fpsSpinBox->value(), 100));
  quint64 const loopMs   = 1000/fps * m_waterfallAvg;
  quint64 const thisLoop = QDateTime::currentMSecsSinceEpoch();

  if (m_lastLoop == 0) m_lastLoop = thisLoop;

  if (quint64 const delta = thisLoop - m_lastLoop;
                    delta > loopMs + 10)
  {
    qDebug() << "widegraph overrun" << (delta - loopMs);
  }

  m_lastLoop = thisLoop;

  // Do the drawing.

  drawSwide();

  // Compute the processing time and adjust loop to hit the next 100ms.

  auto const endLoop        = QDateTime::currentMSecsSinceEpoch();
  auto const processingTime = endLoop - thisLoop;

  m_drawTimer.start(processingTime < loopMs ? static_cast<int>(loopMs - processingTime) : 0);
}

void WideGraph::drawSwide(){
    if(m_paused){
        return;
    }

    QMutexLocker lock(&m_drawLock);
    SWide        swideLocal;

    // draw the tr cycle horizontal lines if needed

    qint64   const now            = DriftingDateTime::currentMSecsSinceEpoch();
    unsigned const secondInToday  = (now % 86400000LL) / 1000;
    int      const secondInPeriod = secondInToday % m_TRperiod;

    if (secondInPeriod < m_lastSecondInPeriod)
    {
      swideLocal.fill(1.0e30f);
      ui->widePlot->draw(swideLocal.data(), true);
    }
#if 0
    else if (m_lastSecondInPeriod != secondInPeriod)
    {
      ui->widePlot->drawHorizontalLine(Qt::white, 0, 5);
    }
#endif
    m_lastSecondInPeriod = secondInPeriod;

    // then, draw the data
    swideLocal = m_swide;
    ui->widePlot->draw(swideLocal.data(), true);
}

void
WideGraph::on_bppSpinBox_valueChanged(int const n)
{
  ui->widePlot->setBinsPerPixel(n);
}

void
WideGraph::on_qsyPushButton_clicked()
{
  emit qsy(rxFreq() - centerFreq());
}

void WideGraph::on_offsetSpinBox_valueChanged(int n){
  if(n == rxFreq()){
      return;
  }

  // TODO: jsherer - here's where we'd set minimum frequency again (later?)
  n = qMax(0, n);

  setRxFreq(n);
  setTxFreq(n);
  setFreq2(n, n);
}

void
WideGraph::on_waterfallAvgSpinBox_valueChanged(int const n)
{
  m_waterfallAvg = n;
  ui->widePlot->setWaterfallAvg(n);
}

void
WideGraph::keyPressEvent(QKeyEvent * event)
{  
  switch (event->key())
  {
  case Qt::Key_F11:
    emit f11f12(11);
    break;
  case Qt::Key_F12:
    emit f11f12(12);
    break;
  default:
    event->ignore();
  }
}

void
WideGraph::setRxFreq(int const n)
{
  ui->widePlot->setRxFreq(n);
  ui->offsetSpinBox->setValue(n);
}

int
WideGraph::rxFreq() const
{
  return ui->widePlot->rxFreq();
}

int
WideGraph::centerFreq() const
{
  return ui->centerSpinBox->value();
}

int
WideGraph::nStartFreq() const
{
  return ui->widePlot->startFreq();
}

int
WideGraph::filterMinimum() const
{   
  return std::max(0, std::min(m_filterMinimum, m_filterMaximum));
}

int
WideGraph::filterMaximum() const
{
  return std::min(std::max(m_filterMinimum, m_filterMaximum), 5000);
}

bool
WideGraph::filterEnabled() const
{
  return m_filterEnabled;
}

void
WideGraph::setFilterCenter(int const n)
{
  auto const delta = n - m_filterCenter;
  setFilter(filterMinimum() + delta,
            filterMaximum() + delta);
}

void
WideGraph::setFilter(int const a,
                     int const b)
{
  auto const low    = std::min(a, b);
  auto const high   = std::max(a, b);
  auto const width  = high - low;
  auto const center = low + width / 2;

  // update the filter history
  m_filterMinimum = a;
  m_filterMaximum = b;
  m_filterCenter  = center;

    // update the spinner UI
  setValueBlocked(a,      ui->filterMinSpinBox);
  setValueBlocked(b,      ui->filterMaxSpinBox);
  setValueBlocked(center, ui->filterCenterSpinBox);
  setValueBlocked(width,  ui->filterWidthSpinBox);

  // update the wide plot UI
  ui->widePlot->setFilterCenter(center);
  ui->widePlot->setFilterWidth(width);
}

void
WideGraph::setFilterMinimumBandwidth(int const width)
{
  m_filterMinWidth = width;

  ui->filterWidthSpinBox->setMinimum(width);

  auto const low  = filterMinimum();
  auto const high = filterMaximum();

  setFilter(low, std::max(low + width, high));
}

void
WideGraph::setFilterEnabled(bool enabled)
{
  m_filterEnabled = enabled;

  // update the filter ui
  ui->filterCenterSpinBox->setEnabled(enabled);
  ui->filterCenterSyncButton->setEnabled(enabled);
  ui->filterWidthSpinBox->setEnabled(enabled);
  ui->filterMinSpinBox->setEnabled(enabled);
  ui->filterMaxSpinBox->setEnabled(enabled);

  // update the checkbox ui
  setValueBlocked(enabled, ui->filterCheckBox);

  // update the wideplot
  ui->widePlot->setFilterEnabled(enabled);
}

void
WideGraph::setFilterOpacityPercent(int const n)
{
  // update the spinbox
  setValueBlocked(n, ui->filterCenterSpinBox);

  // update the wide plot
  ui->widePlot->setFilterOpacity(int((float(n)/100.0)*255));
}

int
WideGraph::fSpan() const
{
  return ui->widePlot->fSpan ();
}

void
WideGraph::setPeriod(int const ntrperiod)
{
  m_TRperiod = ntrperiod;
  ui->widePlot->setPeriod(ntrperiod);
}

void WideGraph::setTxFreq(int n)                                   //setTxFreq
{
  emit setXIT2(n);
  ui->widePlot->setTxFreq(n);
  ui->offsetSpinBox->setValue(n);
}

void
WideGraph::setSubMode(int const n)
{
  ui->widePlot->setSubMode(n);
}

void
WideGraph::on_spec2dComboBox_currentIndexChanged(int const index)
{
  ui->smoSpinBox->setEnabled(false);
  switch (index)
  {
    case 0:
      ui->widePlot->setSpectrum(WF::Spectrum::Current);
      break;
    case 1:
      ui->widePlot->setSpectrum(WF::Spectrum::Cumulative);
      break;
    case 2:
      ui->widePlot->setSpectrum(WF::Spectrum::LinearAvg);
      ui->smoSpinBox->setEnabled(true);
      break;
  }
  replot();
}

void WideGraph::setFreq2(int rxFreq, int txFreq)                  //setFreq2
{
  emit setFreq3(rxFreq,txFreq);
}

void
WideGraph::setDialFreq(double const d)
{
  ui->widePlot->setDialFreq(d);
}

void
WideGraph::setTimeControlsVisible(bool const visible)
{
  setControlsVisible(visible);
  ui->tabWidget->setCurrentWidget(ui->timingTab);
}

bool
WideGraph::timeControlsVisible() const
{
  return controlsVisible() && ui->tabWidget->currentWidget() == ui->timingTab;
}

void
WideGraph::setControlsVisible(bool const visible)
{
  ui->cbControls->setChecked(!visible);
  ui->cbControls->setChecked(visible);
  ui->tabWidget->setCurrentWidget(ui->controlTab);
}

bool
WideGraph::controlsVisible() const
{
  auto sizes = ui->splitter->sizes();
  return ui->cbControls->isChecked() && sizes.last() > 0;
}

void
WideGraph::setBand (QString const & band)
{
  m_rxBand = band;
  ui->widePlot->setBand(band);
}

void
WideGraph::on_fStartSpinBox_valueChanged(int const n)
{
  ui->widePlot->setStartFreq(n);
}

void
WideGraph::readPalette()
{
  try
  {
    if (user_defined == m_waterfallPalette)
    {
      ui->widePlot->setColours (WF::Palette{m_userPalette}.interpolate());
    }
    else
    {
      ui->widePlot->setColours (WF::Palette{m_palettes_path.absoluteFilePath (m_waterfallPalette + ".pal")}.interpolate());
    }
  }
  catch (std::exception const & e)
  {
    MessageBox::warning_message (this, tr("Read Palette"), e.what());
  }
}

QVector<QColor> const &
WideGraph::colors() const
{
  return ui->widePlot->colors();
}

void
WideGraph::on_paletteComboBox_activated(int const palette_index)
{
  m_waterfallPalette = ui->paletteComboBox->itemText(palette_index);
  readPalette();
  replot();
}

void
WideGraph::on_cbFlatten_toggled(bool const b)
{
  m_bFlatten = b;
  ui->widePlot->setFlatten(b);
}

void
WideGraph::on_cbControls_toggled(bool const b)
{
  ui->controls_widget->setVisible(b);

  static int lastSize = ui->splitter->width()/4;  // XXX static
  auto sizes = ui->splitter->sizes();
  if(b){
      ui->splitter->setSizes({ sizes.first(), lastSize });
  } else {
      // keep track of the last size of the control
      lastSize = qMax(sizes.last(), 100);
  }
}

void
WideGraph::on_adjust_palette_push_button_clicked(bool)
{
  try
  {
    if (m_userPalette.design ())
    {
      m_waterfallPalette = user_defined;
      ui->paletteComboBox->setCurrentText(m_waterfallPalette);
      readPalette();
    }
  }
  catch (std::exception const & e)
  {
    MessageBox::warning_message(this, tr("Read Palette"), e.what());
  }
}

bool
WideGraph::flatten() const
{
  return m_bFlatten;
}

void
WideGraph::replot()
{
  if(ui->widePlot->scaleOK()) ui->widePlot->replot();
}

void
WideGraph::on_gainSlider_valueChanged(int const value)
{
  ui->widePlot->setPlotGain(value);
  replot();
}

void WideGraph::on_zeroSlider_valueChanged(int const value)
{
  ui->widePlot->setPlotZero(value);
  replot();
}

void
WideGraph::on_gain2dSlider_valueChanged(int const value) 
{
  ui->widePlot->setPlot2dGain(value);
  if(ui->widePlot->scaleOK())
  {
    ui->widePlot->draw(m_swide.data(), false);
  }
}

void
WideGraph::on_zero2dSlider_valueChanged(int const value)
{
  ui->widePlot->setPlot2dZero(value);
  if(ui->widePlot->scaleOK())
  {
    ui->widePlot->draw(m_swide.data(), false);
  }
}

void
WideGraph::on_smoSpinBox_valueChanged(int const n)
{
  m_nsmo = n;
}

int
WideGraph::smoothYellow() const
{
  return m_nsmo;
}

void
WideGraph::on_sbPercent2dPlot_valueChanged(int const n)
{
  m_Percent2DScreen = n;
  ui->widePlot->setPercent2DScreen(n);
}

void
WideGraph::on_filterMinSpinBox_valueChanged(int const n)
{
  setFilter(n, m_filterMaximum);
}

void
WideGraph::on_filterMaxSpinBox_valueChanged(int const n)
{
  setFilter(m_filterMinimum, n);
}

void
WideGraph::on_filterCenterSpinBox_valueChanged(int const n)
{
  setFilterCenter(n);
}

void
WideGraph::on_filterWidthSpinBox_valueChanged(int const n)
{
  setFilter(m_filterCenter - n/2,
            m_filterCenter - n/2 + n);
}

void
WideGraph::on_filterCenterSyncButton_clicked()
{
  setFilterCenter(ui->offsetSpinBox->value());
}

void
WideGraph::on_filterCheckBox_toggled(bool const b)
{
  setFilterEnabled(b);
}

void
WideGraph::on_filterOpacitySpinBox_valueChanged(int const n)
{
  setFilterOpacityPercent(n);
}

void WideGraph::on_driftSpinBox_valueChanged(int const n)
{
  if (n != DriftingDateTime::drift()) setDrift(n);
}

void WideGraph::on_driftSyncButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = m_TRperiod - (now.time().second() % m_TRperiod);
    int nNeg = (now.time().second() % m_TRperiod) - m_TRperiod;

    if(abs(nNeg) < nPos){
        n = nNeg;
    } else {
        n = nPos;
    }

    setDrift(n * 1000);
}

void WideGraph::on_driftSyncEndButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = m_TRperiod - (now.time().second() % m_TRperiod);
    int nNeg = (now.time().second() % m_TRperiod) - m_TRperiod;

    if(abs(nNeg) < nPos){
        n = nNeg + 2;
    } else {
        n = nPos - 2;
    }

    setDrift(n * 1000);
}

void WideGraph::on_driftSyncMinuteButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();
    int n = 0;
    int s = now.time().second();

    if(s < 30){
        n = -s;
    } else {
        n = 60 - s;
    }

    setDrift(n * 1000);
}

void
WideGraph::on_driftSyncResetButton_clicked()
{
  setDrift(0);
}

void WideGraph::setDrift(int n){
    int prev = drift();

    DriftingDateTime::setDrift(n);

    qDebug() << qSetRealNumberPrecision(12) << "Drift milliseconds:" << n;
    qDebug() << qSetRealNumberPrecision(12) << "Clock time:" << QDateTime::currentDateTimeUtc();
    qDebug() << qSetRealNumberPrecision(12) << "Drifted time:" << DriftingDateTime::currentDateTimeUtc();

    if(ui->driftSpinBox->value() != n){
        ui->driftSpinBox->setValue(n);
    }

    emit drifted(prev, n);
}

int
WideGraph::drift() const
{
  return DriftingDateTime::drift();
}

void WideGraph::setQSYEnabled(bool enabled){
    ui->qsyPushButton->setEnabled(enabled);
    ui->centerSpinBox->setEnabled(enabled);
}
