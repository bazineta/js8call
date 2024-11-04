#include "MessageClient.hpp"
#include <stdexcept>
#include <QApplication>
#include <QByteArray>
#include <QHostAddress>
#include <QHostInfo>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>

#include "DriftingDateTime.h"
#include "pimpl_impl.hpp"
#include "moc_MessageClient.cpp"

/******************************************************************************/
// Constants
/******************************************************************************/

namespace
{
  constexpr auto PING_INTERVAL = std::chrono::seconds(15);
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

class MessageClient::impl final : public QUdpSocket
{
  Q_OBJECT

public:

  // Constructor

  impl(quint16   const port,
       MessageClient * self)
    : self_ {self}
    , port_ {port}
    , ping_ {new QTimer {this}}
  {
    connect(ping_, &QTimer::timeout,      this, &impl::ping);
    connect(this,  &QIODevice::readyRead, this, [this]()
    {
      while (hasPendingDatagrams())
      {
        QByteArray datagram(pendingDatagramSize(), Qt::Uninitialized);
        
        if (readDatagram(datagram.data(),
                         datagram.size()) > 0)
        {
          try
          {
            Q_EMIT self_->message (Message::fromJson(datagram));
          }
          catch (std::exception const & e)
          {
            Q_EMIT self_->error (QString {"MessageClient exception: %1"}.arg(e.what()));
          }
          catch (...)
          {
            Q_EMIT self_->error (QString {"Unexpected exception in MessageClient"});
          }
        }
      }
    });

    ping_->start(PING_INTERVAL);

    bind();
  }

  // Destructor

  ~impl()
  {
    abort_host_lookup();

    if (port_ && !host_.isNull())
    {
      send_message({"CLOSE"});
    }
  }

  // Send a ping message, if we have a valid port and host.

  void
  ping()
  {
    if (port_ && !host_.isNull())
    {
      send_message({"PING", "", {
        {"NAME",    QVariant(QApplication::applicationName())},
        {"VERSION", QVariant(QApplication::applicationVersion())},
        {"UTC",     QVariant(DriftingDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
      }});
    }
  }

  // If the JSON-serialized form of the message isn't exactly the same as
  // the one that we last sent, send it and note it as the prior datagram
  // sent.
  //
  // Caller is required to make the determination that our port and host
  // are valid prior to calling this function.

  void
  send_message(Message const & message)
  {
    if (auto const datagram  = message.toJson();
                   datagram != lastDatagram_)
    {
      writeDatagram(datagram, host_, port_);
      lastDatagram_ = datagram;
    }
  }

  // If we've got a host lookup in flight, but not yet completed, abort it
  // and indicate that we no longer have one in flight.

  void
  abort_host_lookup()
  {
    if (hostLookupId_ != -1)
    {
      QHostInfo::abortHostLookup(hostLookupId_);
      hostLookupId_ = -1;
    }
  }

  // Abort any current host lookup that might be in flight, and start a new
  // host lookup for the provided server name, noting that we have a lookup
  // in flight.
  //
  // If, at the time of host lookup completion, we find ourselves to be the
  // active host lookup, and we were able to look up addresses, then use the
  // first address associated with the server as our host address, and send
  // a ping.
  //
  // No matter the result of the host lookup, we're going to drain the queue,
  // either via sending messages if the host lookup worked, or by clearing it
  // if the lookup failed.

  void
  queue_host_lookup(QString const & name)
  {
    abort_host_lookup();

    hostLookupId_ = QHostInfo::lookupHost(name,
                                          this,
                                          [this](QHostInfo const & info)
    {
      // This functor is always called in the context of the thread that
      // made the call to lookupHost(), so we're safe to modify anything
      // that we were safe to modify outside.

      if (info.lookupId() == hostLookupId_)
      {
        hostLookupId_ = -1;

        if (auto const & list = info.addresses();
                        !list.isEmpty())
        {
          host_ = list.first();

          qDebug() << "MessageClient Host:" << host_.toString()
                   << "loopback:"           << host_.isLoopback()
                   << "multicast:"          << host_.isMulticast();

          ping();

          if (port_ && !host_.isNull())
          {
            while (!messageQueue_.isEmpty()) send_message(messageQueue_.dequeue());
          }
        }
        else
        {
          Q_EMIT self_->error (QString {"UDP server lookup failed: %1"}.arg(info.errorString()));
          messageQueue_.clear();
        }
      }
    });
  }

  // Data members

  MessageClient * self_;
  quint16         port_;
  QTimer        * ping_;
  QHostAddress    host_;
  int             hostLookupId_ = -1;
  QQueue<Message> messageQueue_;
  QByteArray      lastDatagram_;
};

/******************************************************************************/
// Implementation
/******************************************************************************/

#include "MessageClient.moc"

// Constructor
//
// On Windows, Qt will seemingly emit spurious 'connection refused' errors
// for UDP sockets; ignore them if they appear.

MessageClient::MessageClient(QString const & name,
                             quint16 const   port,
                             QObject       * parent)
  : QObject {parent}
  , m_      {port, this}
{
  connect(&*m_, &impl::errorOccurred, [this](impl::SocketError e)
  {
#if defined (Q_OS_WIN)
    if (e != impl::NetworkError &&
        e != impl::ConnectionRefusedError)
#else
    Q_UNUSED (e);
#endif
    {
      Q_EMIT error (m_->errorString());
    }
  });

  set_server_name(name);
}

// Host accessor; returns host information for the server. Note
// that this will return an invalid object while a host lookup
// is in flight, so consider thread inertia before relying on
// this; the case may be that information is coming, but it's
// not here just yet.

QHostAddress
MessageClient::server_host() const
{
  return m_->host_;
}

// Port accessor; if zero, anything sent to us will be dropped
// on the floor.

quint16
MessageClient::server_port() const
{
  return m_->port_;
}

// Set our server name. If the name is empty, then what happens to
// messages provided to us via send() depends on what our port is
// set to. If it's zero, they'll just be dropped on the floor. If
// it's non-zero, they'll be queued until this method is called
// again with a non-empty name.

void
MessageClient::set_server_name(QString const & name)
{
  m_->host_.clear();

  if (name.isEmpty()) m_->abort_host_lookup();
  else                m_->queue_host_lookup(name);
}

// Set our port; if zero, no sending will be performed, and anything
// sent us for disposition via send() will be dropped on the floor.

void
MessageClient::set_server_port(quint16 const port)
{
  m_->port_ = port;
}

// If we've got a port, i.e., we're supposed to send messages, then queue
// the message for later transmission if we don't have a host yet; attempt
// to send it immediately if we've got a host.

void
MessageClient::send(Message const & message)
{
  if (m_->port_)
  {
    if (m_->host_.isNull()) m_->messageQueue_.enqueue(message);
    else                    m_->send_message(message);
  }
}

// Send a raw datagram to the host and port specified, if they've got sane
// values. We're just leveraging the fact that we've got a socket to play
// with; this is, in short, just a convenient way to obtain general UDP
// messaging, unrelated to everything above.

void
MessageClient::send_raw_datagram(QByteArray   const & datagram,
                                 QHostAddress const & host,
                                 quint16      const   port)
{
  if (port && !host.isNull())
  {
    m_->writeDatagram(datagram, host, port);
  }
}

/******************************************************************************/
