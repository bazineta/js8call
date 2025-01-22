#ifndef __JS8
#define __JS8

#include <functional>
#include <string>
#include <QObject>
#include <QScopedPointer>
#include <QThread>

namespace JS8
{
  Q_NAMESPACE

  namespace SyncStats
  {
    using Candidate = std::function<void(int, float, float, float)>;
    using Processed = std::function<void(int, float, float, float)>;
  }

  using Detected = std::function<void(int, int, float, float, std::string, int, float, int)>;

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

      void syncStatsCandidate(int, float, float, float);
      void syncStatsProcessed(int, float, float, float);
      void detected(int, int, float, float, std::string, int, float, int);
      void decodeDone();

  private:

    QScopedPointer<Worker> m_worker;
    QThread                m_thread;
  };
}

#endif