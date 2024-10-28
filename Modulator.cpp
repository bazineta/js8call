#include "Modulator.hpp"
#include <cmath>
#include <limits>
#include <QDateTime>
#include <QDebug>
#include <QtMath>
#include "commons.h"
#include "DriftingDateTime.h"
#include "JS8Submode.hpp"
#include "mainwindow.h"
#include "soundout.h"

#include "moc_Modulator.cpp"

namespace
{
  constexpr double TAU        = 2 * M_PI;
  constexpr auto   FRAME_RATE = 48000;
  constexpr auto   MS_PER_DAY = 86400000;
  constexpr auto   MS_PER_SEC = 1000;
}

void
Modulator::start(double        const frequency,
                 int           const submode,
                 SoundOutput * const stream,
                 Channel       const channel)
{
  Q_ASSERT (stream);

  if (m_state != State::Idle) stop();

  m_quickClose   = false;
  m_isym0        = std::numeric_limits<unsigned>::max(); // big number
  m_frequency0   = 0.;
  m_phi          = 0.;
  m_nsps         = JS8::Submode::symbolSamples(submode);
  m_frequency    = frequency;
  m_amp          = std::numeric_limits<qint16>::max();
  m_toneSpacing  = JS8::Submode::toneSpacing(submode);
  m_silentFrames = 0;
  m_ic           = 0;

  // If we're not tuning, then we'll need to figure out exactly when we
  // should start transmitting; this will depend on the submode in play.

  if (!m_tuning)
  {
    // Get current time in milliseconds from the perspective of this machine.
    // Using the submode-specific transmit period in milliseconds, determine
    // the number of milliseconds that we're presently at into the nominal
    // transmit start time.

    auto     const startDelayMS = JS8::Submode::startDelayMS(submode);
    unsigned const mstr         = (DriftingDateTime::currentMSecsSinceEpoch() % MS_PER_DAY) %
                                  (JS8::Submode::period(submode)              * MS_PER_SEC);

    // Calculate number of silent frames to send, so that audio will
    // start at the nominal time "delay_ms" into the Tx sequence.

    if (startDelayMS > mstr)
    {
      m_silentFrames = (startDelayMS - mstr) * FRAME_RATE / 1000;
    }

    // Adjust for late starts.
    
    if (!m_silentFrames && mstr >= startDelayMS)
    {
      m_ic = (mstr - startDelayMS) * FRAME_RATE / 1000;
    }
  }

  initialize(QIODevice::ReadOnly, channel);

  Q_EMIT stateChanged ((m_state = m_silentFrames
                                ? State::Synchronizing
                                : State::Active));

  m_stream = stream;

  if (m_stream)
  {
    m_stream->restart(this);
  }
  else
  {
    qDebug() << "Modulator::start: no audio output stream assigned";
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

  if (m_state != State::Idle)
  {
    Q_EMIT stateChanged ((m_state = State::Idle));
  }

  AudioDevice::close();
}

qint64
Modulator::readData(char * const data,
                    qint64 const maxSize)
{
  if (maxSize == 0) return 0;

  Q_ASSERT (!(maxSize % qint64(bytesPerFrame()))); // no torn frames
  Q_ASSERT (isOpen());

  qint64               framesGenerated = 0;
  qint64         const maxFrames       = maxSize / bytesPerFrame();
  qint16       *       samples         = reinterpret_cast<qint16 *>(data);
  qint16 const * const samplesEnd      = samples + maxFrames * (bytesPerFrame() / sizeof(qint16));

  switch (m_state)
  {
    case State::Synchronizing:
    {
      if (m_silentFrames)
      {
        // Send silence up to end of start delay.

        framesGenerated = qMin(m_silentFrames, maxFrames);

        do
        {
          samples = load(0, samples); // silence
        } while (--m_silentFrames && samples != samplesEnd);

        if (!m_silentFrames)
        {
          Q_EMIT stateChanged ((m_state = State::Active));
        }
      }
    }
    [[fallthrough]];

    case State::Active:
    {
      double const baud = 12000.0 / m_nsps;
      unsigned int i0; // fade out parameters, no
      unsigned int i1; // fade out for tuning

      if (m_tuning)
      {
        i1 = i0 = 9999 * m_nsps;
      }
      else
      {
        i0 = (JS8_NUM_SYMBOLS - 0.017) * 4.0 * m_nsps;
        i1 =  JS8_NUM_SYMBOLS          * 4.0 * m_nsps;
      }

      qint16  sample;

      while (samples != samplesEnd && m_ic <= i1)
      {
        unsigned int const isym = m_tuning ? 0 : m_ic / (4.0 * m_nsps);   //Actual fsample=48000

        if (isym != m_isym0 || m_frequency != m_frequency0)
        {
          double toneFrequency;

          if (itone[0] >= 100)
          {
            toneFrequency = itone[0];
          }
          else
          {
            toneFrequency = m_toneSpacing == 0.0
                          ? m_frequency + itone[isym] * baud
                          : m_frequency + itone[isym] * m_toneSpacing;
          }
          m_dphi       = TAU * toneFrequency / FRAME_RATE;
          m_isym0      = isym;
          m_frequency0 = m_frequency;         //???
        }

        m_phi += m_dphi;
        if (m_phi > TAU) m_phi -= TAU;
        if (m_ic  > i0)  m_amp  = 0.98 * m_amp;
        if (m_ic  > i1)  m_amp  = 0.0;

        sample  = qRound(m_amp * qSin(m_phi));
        samples = load(sample, samples);

        ++framesGenerated;
        ++m_ic;
      }

       // TODO G4WJS: compare double with zero might not be wise

      if (m_amp == 0.0)
      {
        Q_EMIT stateChanged ((m_state = State::Idle));
        return framesGenerated * bytesPerFrame();
        m_phi = 0.0;
      }

      m_frequency0 = m_frequency;

      // done for this chunk - continue on next call

      while (samples != samplesEnd)  // pad block with silence
      {
        samples = load(0, samples);
        ++framesGenerated;
      }

      return framesGenerated * bytesPerFrame();
    }
    [[fallthrough]];

    case State::Idle:
    break;
  }

  Q_ASSERT (State::Idle == m_state);
  return 0;
}
