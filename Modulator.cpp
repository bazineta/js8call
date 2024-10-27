#include "Modulator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>
#include <QtMath>
#include "commons.h"
#include "DriftingDateTime.h"
#include "mainwindow.h"
#include "soundout.h"

#include "moc_Modulator.cpp"

extern float gran();		// Noise generator (for tests only)

namespace
{
  constexpr double TAU = 2 * M_PI;
  
  unsigned
  delayMS(qint32 const trPeriod)
  {
    switch (trPeriod)
    {
      case JS8A_TX_SECONDS: { return JS8A_START_DELAY_MS; }
      case JS8B_TX_SECONDS: { return JS8B_START_DELAY_MS; }
      case JS8C_TX_SECONDS: { return JS8C_START_DELAY_MS; }
      case JS8E_TX_SECONDS: { return JS8E_START_DELAY_MS; }
      case JS8I_TX_SECONDS: { return JS8I_START_DELAY_MS; }
      default:              { return 0;                   }
    }
  }
}

Modulator::Modulator(unsigned  frameRate,
                     unsigned  periodLengthInSeconds,
                     QObject * parent)
  : AudioDevice      {parent}
  , m_quickClose     {false}
  , m_phi            {0.0}
  , m_toneSpacing    {0.0}
  , m_fSpread        {0.0}
  , m_frameRate      {frameRate}
  , m_period         {periodLengthInSeconds}
  , m_state          {Idle}
  , m_tuning         {false}
  , m_j0             {-1}
  , m_toneFrequency0 {1500.0}
{
}

void
Modulator::start(unsigned      symbolsLength,
                 double        framesPerSymbol,
                 double        frequency,
                 double        toneSpacing,
                 SoundOutput * stream,
                 Channel       channel,
                 bool          synchronize,
                 bool          fastMode,
                 double        dBSNR,
                 int           TRperiod)
{
  // qDebug () << "mode:" << mode << "symbolsLength:" << symbolsLength << "framesPerSymbol:" << framesPerSymbol << "frequency:" << frequency << "toneSpacing:" << toneSpacing << "channel:" << channel << "synchronize:" << synchronize << "fastMode:" << fastMode << "dBSNR:" << dBSNR << "TRperiod:" << TRperiod;
  Q_ASSERT (stream);

  // Time according to this computer which becomes our base time

  qint64   const ms0  = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
  unsigned const mstr = ms0 % int(1000.0 * m_period); // ms into the nominal Tx start time

  if(m_state != Idle) stop();

  m_quickClose    = false;
  m_symbolsLength = symbolsLength;
  m_isym0         = std::numeric_limits<unsigned>::max(); // big number
  m_frequency0    = 0.;
  m_phi           = 0.;
  m_addNoise      = dBSNR < 0.;
  m_nsps          = framesPerSymbol;
  m_frequency     = frequency;
  m_amp           = std::numeric_limits<qint16>::max();
  m_toneSpacing   = toneSpacing;
  m_bFastMode     = fastMode;
  m_TRperiod      = TRperiod;

  unsigned const delay_ms = delayMS(m_TRperiod);

  // noise generator parameters
  if (m_addNoise)
  {
    m_snr = qPow (10.0, 0.05 * (dBSNR - 6.0));
    m_fac = 3000.0;
    if (m_snr > 1.0) m_fac = 3000.0 / m_snr;
  }

  m_silentFrames = 0;
  m_ic           = 0;

  if (!m_tuning && !m_bFastMode)
  {
    // Calculate number of silent frames to send, so that audio will
    // start at the nominal time "delay_ms" into the Tx sequence.

    if (synchronize)
    {
      if(delay_ms > mstr) m_silentFrames = (delay_ms - mstr) * m_frameRate / 1000;
    }

    // Adjust for late starts.
    
    if(!m_silentFrames && mstr >= delay_ms)
    {
      m_ic = (mstr - delay_ms) * m_frameRate / 1000;
    }
  }

  initialize(QIODevice::ReadOnly, channel);

  Q_EMIT stateChanged ((m_state = (synchronize && m_silentFrames) ?
                        Synchronizing : Active));

  // qDebug() << "delay_ms:" << delay_ms << "mstr:" << mstr << "m_silentFrames:" << m_silentFrames << "m_ic:" << m_ic << "m_state:" << m_state;

  m_stream = stream;
  if (m_stream)
  {
    m_stream->restart (this);
  }
  else
  {
    qDebug () << "Modulator::start: no audio output stream assigned";
  }
}

void
Modulator::tune(bool const tuning)
{
  m_tuning = tuning;
  if (!m_tuning) stop(true);
}

void
Modulator::stop(bool const quickClose)
{
  m_quickClose = quickClose;
  close();
}

void
Modulator::close()
{
  if (m_stream)
  {
    if (m_quickClose) m_stream->reset();
    else              m_stream->stop();
  }

  if (m_state != Idle)
  {
    Q_EMIT stateChanged ((m_state = Idle));
  }

  AudioDevice::close();
}

qint64
Modulator::readData(char * const data,
                    qint64 const maxSize)
{
  double toneFrequency = 1500.0;
  
  if(m_nsps == 6)
  {
    toneFrequency = 1000.0;
    m_frequency   = 1000.0;
    m_frequency0  = 1000.0;
  }

  if (maxSize == 0) return 0;

  Q_ASSERT (!(maxSize % qint64 (bytesPerFrame ()))); // no torn frames
  Q_ASSERT (isOpen ());

  qint64   numFrames       = maxSize / bytesPerFrame();
  qint16 * samples         = reinterpret_cast<qint16 *>(data);
  qint16 * end             = samples + numFrames * (bytesPerFrame() / sizeof(qint16));
  qint64   framesGenerated = 0;

  switch (m_state)
  {
    case Synchronizing:
    {
      if (m_silentFrames)
      {
        // Send silence up to end of start delay.

        framesGenerated = qMin(m_silentFrames, numFrames);

        do
        {
          samples = load(0, samples); // silence
        } while (--m_silentFrames && samples != end);

        if (!m_silentFrames)
        {
          Q_EMIT stateChanged ((m_state = Active));
        }
      }
    }
    [[fallthrough]];

    case Active:
    {
      double const baud = 12000.0 / m_nsps;
      unsigned int i0; // fade out parameters, no
      unsigned int i1; // fade out for tuning

      if (m_tuning)
      {
        i1 = i0 = (m_bFastMode ? 999999 : 9999) * m_nsps;
      }
      else
      {
        i0 = (m_symbolsLength - 0.017) * 4.0 * m_nsps;
        i1 =  m_symbolsLength          * 4.0 * m_nsps;
      }

      if(m_bFastMode and !m_tuning)
      {
        i1 = m_TRperiod*48000 - 24000;
        i0 = i1-816;
      }

      qint16       sample;
      unsigned int isym;

      while (samples != end && m_ic <= i1)
      {
        isym = 0;
        if (!m_tuning and m_TRperiod!=3) isym=m_ic / (4.0 * m_nsps);   //Actual fsample=48000
        if (m_bFastMode) isym=isym%m_symbolsLength;
        if (isym != m_isym0 || m_frequency != m_frequency0)
        {
          if (itone[0] >= 100)
          {
            m_toneFrequency0 = itone[0];
          }
          else
          {
            if (m_toneSpacing==0.0) m_toneFrequency0 = m_frequency + itone[isym] * baud;
            else                    m_toneFrequency0 = m_frequency + itone[isym] * m_toneSpacing;
          }
          m_dphi       = TAU * m_toneFrequency0 / m_frameRate;
          m_isym0      = isym;
          m_frequency0 = m_frequency;         //???
        }

        int const j = m_ic / 480;

        if (m_fSpread > 0.0 and j != m_j0)
        {
          float const x1 = QRandomGenerator::global()->generateDouble();
          float const x2 = QRandomGenerator::global()->generateDouble();
          toneFrequency  = m_toneFrequency0 + 0.5 * m_fSpread * (x1 + x2 - 1.0);
          m_dphi         = TAU * toneFrequency / m_frameRate;
          m_j0           = j;
        }

        m_phi += m_dphi;
        if (m_phi > TAU) m_phi -= TAU;
        if (m_ic  > i0)  m_amp  = 0.98 * m_amp;
        if (m_ic  > i1)  m_amp  = 0.0;

        sample  = qRound(m_amp * qSin(m_phi));
        samples = load(postProcessSample(sample), samples);

        ++framesGenerated;
        ++m_ic;
      }

       // TODO G4WJS: compare double with zero might not be wise

      if (m_amp == 0.0)
      {
        Q_EMIT stateChanged ((m_state = Idle));
        return framesGenerated * bytesPerFrame();
        m_phi = 0.0;
      }

      m_frequency0 = m_frequency;

      // done for this chunk - continue on next call

      while (samples != end)  // pad block with silence
      {
        samples = load(0, samples);
        ++framesGenerated;
      }

      return framesGenerated * bytesPerFrame();
    }
    [[fallthrough]];

    case Idle:
    break;
  }

  Q_ASSERT (Idle == m_state);
  return 0;
}

// If this is a test frame, we'll add noise.

qint16
Modulator::postProcessSample(qint16 sample) const
{
  if (m_addNoise)
  {
    return std::clamp(static_cast<qint32>(m_fac * (gran() + sample * m_snr / 32768.0)),
                      static_cast<qint32>(std::numeric_limits<qint16>::min()),
                      static_cast<qint32>(std::numeric_limits<qint16>::max()));
  }

  return sample;
}
