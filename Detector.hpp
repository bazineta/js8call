#ifndef DETECTOR_HPP__
#define DETECTOR_HPP__
#include "AudioDevice.hpp"
#include <array>
#include <vendor/Eigen/Dense>
#include <QMutex>

//
// output device that distributes data in predefined chunks via a signal
//
// the underlying device for this abstraction is just the buffer that
// stores samples throughout a receiving period
//
class Detector : public AudioDevice
{
  Q_OBJECT;

  // Amount we're going to downsample; a factor of 4, i.e.,
  // 48kHz to 12kHz, and number of taps in the FIR lowpass
  // filter we're going to use for the downsample process.
  // These together result in the amount to shift data in
  // the FIR filter each time we input a new sample.

  static constexpr std::size_t NDOWN = 48 / 12;
  static constexpr std::size_t NTAPS = 49;
  static constexpr std::size_t SHIFT = NTAPS - NDOWN;

  // Size of a maximally-sized buffer.

  static constexpr std::size_t MaxBufferSize = 7 * 512;

  // A De-interleaved sample buffer big enough for all the
  // samples for one increment of data (a signals worth) at
  // the input sample rate, a mapping for a sample window in
  // the buffer, and the vectors we'll use for the FIR filter.

  using Buffer = std::array<short, MaxBufferSize * NDOWN>;
  using Sample = Eigen::Map<Eigen::Vector<short, NDOWN> const>;
  using Vector = Eigen::Vector<float, NTAPS>;

public:

  // Constructor

  Detector(unsigned  frameRate,
           unsigned  periodLengthInSeconds,
           QObject * parent = nullptr);

  // Inline accessors

  unsigned period() const { return m_period; }

  // Inline manipulators

  QMutex * getMutex()              { return &m_lock; }
  void     setTRPeriod(unsigned p) { m_period = p;   }

  // Accessors

  unsigned secondInPeriod () const;

  // Manipulators

  void clear();
  bool reset() override;
  void resetBufferContent();
  void resetBufferPosition();

  // Signals and slots

  Q_SIGNAL void framesWritten(qint64) const;
  Q_SLOT   void setBlockSize(unsigned);

protected:

  // We don't produce data; we're a sink for it.

  qint64 readData (char       *, qint64) override { return -1; }
  qint64 writeData(char const *, qint64) override;

private:

  // Data members
  
  unsigned                 m_frameRate;
  unsigned                 m_period;
  QMutex                   m_lock;
  Eigen::Map<Vector const> m_w;
  Vector                   m_t;
  Buffer                   m_buffer;
  Buffer::size_type        m_bufferPos     = 0;
  std::size_t              m_samplesPerFFT = MaxBufferSize;
  qint32                   m_ns            = 999;
};

#endif
