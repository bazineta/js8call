#ifndef __JS8
#define __JS8

#include <functional>
#include <string>
#include <variant>
#include <QObject>
#include <QScopedPointer>
#include <QThread>

namespace JS8
{
  Q_NAMESPACE

  namespace Event
  {
    struct DecodeStarted
    {
      int submode;
      int submodes;
    };

    struct SyncStart
    {
      int position;
      int size;
    };

    struct SyncCandidate
    {
      int   mode;
      float frequency;
      float dt;
    };

    struct SyncDecode
    {
      int   mode;
      float frequency;
      float dt;
    };

    struct Decoded
    {
      int         utc;
      int         snr;
      float       xdt;
      float       frequency;
      std::string data;
      int         type;
      float       quality;
      int         mode;
    };

    struct DecodeFinished
    {
      int decoded;
    };

    using Impl = std::variant<DecodeStarted, SyncStart, SyncCandidate, SyncDecode, Decoded, DecodeFinished>;

    using Emit = std::function<void(Impl const &)>;
  }

  class Worker;

  class Decoder: public QObject
  {
      Q_OBJECT
  public:
      Decoder(QObject * parent = nullptr);
      ~Decoder();

  public slots:

      void start(QThread::Priority priority);
      void quit();
      bool wait();
      void decode();

  signals:

      void decodeEvent(Event::Impl const &);
      void decodeDone();

  private:

    QScopedPointer<Worker> m_worker;
    QThread                m_thread;
  };
}

#endif