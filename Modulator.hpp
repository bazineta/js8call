#ifndef MODULATOR_HPP__
#define MODULATOR_HPP__

#include <QAudio>
#include <QPointer>
#include "AudioDevice.hpp"

class SoundOutput;

//
// Input device that generates PCM audio frames that encode a message
// and an optional CW ID.
//
// Output can be muted while underway, preserving waveform timing when
// transmission is resumed.
//
class Modulator
  : public AudioDevice
{
  Q_OBJECT;

public:

  enum class State
  {
    Synchronizing,
    Active,
    Idle
  };

  // Constructor

  Modulator(unsigned  frameRate,
            unsigned  periodLengthInSeconds,
            QObject * parent = nullptr);

  // Inline accessors

  bool   isActive () const { return m_state != State::Idle; }
  bool   isTuning()  const { return m_tuning;               }
  double frequency() const { return m_frequency;            }

  // Inline manipulators

  void setSpread  (double   s) { m_fSpread       = s; }
  void setTRPeriod(unsigned p) { m_period        = p; }
  void set_nsym   (int      n) { m_symbolsLength = n; }

  // Manipulators

  void close() override;

  // Signals

  Q_SIGNAL void stateChanged(State) const;

public slots:

  void setFrequency(double newFrequency) { m_frequency = newFrequency; }

  void start(unsigned symbolsLength,
             double   framesPerSymbol,
             double   frequency,
             double   toneSpacing,
             SoundOutput *,
             Channel            = Mono,
             bool   synchronize = true,
             double dBSNR       = 99.,
             int    TRperiod    = 60);
  void stop(bool quick = false);
  void tune(bool newState = true);

protected:

  qint64 readData (char       *, qint64) override;
  qint64 writeData(char const *, qint64) override
  {
    return -1;			// we don't consume data
  }

#if defined(Q_OS_WIN)
// On Windows, bytesAvailable() must return a size that exceeds some threshold 
// in order for the AudioSink to go into Active state and start pulling data.
// See: https://bugreports.qt.io/browse/QTBUG-108672
  qint64 bytesAvailable () const
  {
    return 8000;
  }
#endif

private:

  // Accessors

  qint16 postProcessSample (qint16 sample) const;

  // Data members

  QPointer<SoundOutput> m_stream;
  State                 m_state          = State::Idle;
  double                m_phi            = 0.0;
  double                m_toneSpacing    = 0.0;
  double                m_fSpread        = 0.0;
  double                m_toneFrequency0 = 1500.0;
  double                m_dphi;
  double                m_amp;
  double                m_nsps;
  double                m_frequency;
  double                m_frequency0;
  double                m_snr;
  double                m_fac;
  qint64                m_silentFrames;
  qint32                m_TRperiod;
  unsigned              m_ic;
  unsigned              m_isym0;
  unsigned              m_symbolsLength;
  unsigned              m_frameRate;
  unsigned              m_period;
  int                   m_j0             = -1;
  bool                  m_quickClose     = false;
  bool                  m_tuning         = false;
  bool                  m_addNoise;
};

#endif
