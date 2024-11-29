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

public:
  //
  // if the data buffer were not global storage and fixed size then we
  // might want maximum size passed as constructor arguments
  //
  // we down sample by a factor of 4
  //
  // the samplesPerFFT argument is the number after down sampling
  //
  Detector (unsigned frameRate, unsigned periodLengthInSeconds, QObject * parent = nullptr);

  QMutex * getMutex(){ return &m_lock; }
  unsigned period() const {return m_period;}
  void setTRPeriod(unsigned p) {m_period=p;}
  bool reset () override;

  Q_SIGNAL void framesWritten (qint64) const;
  Q_SLOT   void setBlockSize (unsigned);

  void clear ();		// discard buffer contents
  void resetBufferPosition();
  void resetBufferContent();

  unsigned secondInPeriod () const;

  static constexpr std::size_t NDOWN = 4;
  static constexpr std::size_t NTAPS = 49;

protected:
  qint64 readData (char * /* data */, qint64 /* maxSize */) override
  {
    return -1;			// we don't produce data
  }

  qint64 writeData (char const * data, qint64 maxSize) override;

private:

  // Size of a maximally-sized buffer.

  static constexpr std::size_t MAXBS = 7 * 512;

  // Amount we shift each time we put a new sample into the FIR.

  static constexpr std::size_t SHIFT = NTAPS - NDOWN;

  // De-interleaved sample buffer big enough for all the
  // samples for one increment of data (a signals worth)
  // at the input sample rate.

  using Buffer = std::array<short, MAXBS * NDOWN>;
  using Vector = Eigen::Vector<float, NTAPS>;
  
  unsigned                 m_frameRate;
  unsigned                 m_period;
  QMutex                   m_lock;
  Eigen::Map<Vector const> m_w;
  Vector                   m_t;
  Buffer                   m_buffer;
  Buffer::size_type        m_bufferPos     = 0;
  std::size_t              m_samplesPerFFT = MAXBS;
  qint32                   m_ns            = 999;
};

#endif
