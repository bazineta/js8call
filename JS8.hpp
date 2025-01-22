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

  namespace Message
  {
    struct Candidate
    {
      int   mode;
      float frequency;
      float dt;
    };

    struct Processed
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

    using Impl = std::variant<Candidate, Processed, Decoded>;

    using Processor = std::function<void(Impl const &)>;
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

      void syncStatsCandidate(Message::Candidate const &);
      void syncStatsProcessed(Message::Processed const &);
      void decoded(Message::Decoded const &);
      void decodeDone();

  private:

    QScopedPointer<Worker> m_worker;
    QThread                m_thread;
  };
}

#endif