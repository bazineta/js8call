// -*- Mode: C++ -*-
#ifndef SOUNDOUT_H__
#define SOUNDOUT_H__

#include <QObject>
#include <QString>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>

// An instance of this sends audio data to a specified soundcard.

class SoundOutput
  : public QObject
{
  Q_OBJECT;
  
public:
  SoundOutput ()
    : m_msBuffered {0u}
    , m_volume {1.0}
  {
  }

  qreal attenuation () const;
  QAudioFormat format() const;

public Q_SLOTS:
  void setFormat (QAudioDevice const& device, unsigned channels, unsigned msBuffered = 0u);
  void setDeviceFormat (QAudioDevice const& device, QAudioFormat const&format, unsigned channels, unsigned msBuffered = 0u);
  void restart (QIODevice *);
  void suspend ();
  void resume ();
  void reset ();
  void stop ();
  void setAttenuation (qreal);	/* unsigned */
  void resetAttenuation ();	/* to zero */
  
Q_SIGNALS:
  void error (QString message) const;
  void status (QString message) const;

private:
  bool audioError () const;

private Q_SLOTS:
  void handleStateChanged (QAudio::State);

private:
  QScopedPointer<QAudioSink> m_stream;
  QAudioFormat m_format;
  unsigned m_msBuffered;
  qreal m_volume;
};

#endif
