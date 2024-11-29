#ifndef DETECTOR_HPP__
#define DETECTOR_HPP__
#include "AudioDevice.hpp"
#include <QScopedArrayPointer>
#include <QMutex>
#include <QMutexLocker>

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
  Detector (unsigned frameRate, unsigned periodLengthInSeconds, QObject * parent = 0);

  QMutex * getMutex(){ return &m_lock; }
  unsigned period() const {return m_period;}
  void setTRPeriod(unsigned p) {m_period=p;}
  bool reset () override;

  Q_SIGNAL void framesWritten (qint64) const;
  Q_SLOT void setBlockSize (unsigned);

  void clear ();		// discard buffer contents
  void resetBufferPosition();
  void resetBufferContent();

  unsigned secondInPeriod () const;

protected:
  qint64 readData (char * /* data */, qint64 /* maxSize */) override
  {
    return -1;			// we don't produce data
  }

  qint64 writeData (char const * data, qint64 maxSize) override;

private:
  unsigned m_frameRate;
  unsigned m_period;
  qint32 m_samplesPerFFT;	// after any down sampling
  qint32 m_ns;
  static size_t const max_buffer_size {7 * 512};
  QScopedArrayPointer<short> m_buffer; // de-interleaved sample buffer
  // big enough for all the
  // samples for one increment of
  // data (a signals worth) at
  // the input sample rate
  size_t m_bufferPos;
  QMutex m_lock;
};

#endif
