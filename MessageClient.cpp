#include "MessageClient.hpp"
#include <algorithm>
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
  constexpr auto PING_MS = 15 * 1000;
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

class MessageClient::impl final : public QUdpSocket
{
  Q_OBJECT

public:

  // Constructor

  impl(port_type const port,
       MessageClient * self)
    : self_ {self}
    , port_ {port}
    , ping_ {new QTimer {this}}
  {
    connect (ping_, &QTimer::timeout,      this, &impl::ping);
    connect (this,  &QIODevice::readyRead, this, &impl::pending_datagrams);

    ping_->start(PING_MS);

    bind();
  }

  // Destructor

  ~impl ()
  {
    if (port_ && !host_.isNull())
    {
      emit_message(Message{"CLOSE"}.toJson());
    }

    if (hostLookupId_ != -1)
    {
      QHostInfo::abortHostLookup(hostLookupId_);
    }
  }

  // Send a ping message, if we have a valid port and host.

  void
  ping()
  {
    if (port_ && !host_.isNull())
    {
      emit_message(Message{"PING", "", {
        {"NAME",    QVariant(QApplication::applicationName())},
        {"VERSION", QVariant(QApplication::applicationVersion())},
        {"UTC",     QVariant(DriftingDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
      }}.toJson());
    }
  }

  // Called when our device is ready to read; attempt to read and process
  // pending datagrams.

  void
  pending_datagrams()
  {
    while (hasPendingDatagrams())
    {
      QByteArray datagram;

      datagram.resize(pendingDatagramSize());
      
      if (readDatagram(datagram.data(),
                       datagram.size()) > 0)
      {
        try
        {
          QJsonParseError parse;
          QJsonDocument   document = QJsonDocument::fromJson(datagram, &parse);

          if (parse.error)
          {
            Q_EMIT self_->error(QString {"MessageClient json parse error: %1"}.arg(parse.errorString()));
            continue;
          }

          if (!document.isObject())
          {
            Q_EMIT self_->error(QString {"MessageClient json parse error: json is not an object"});
            continue;
          }

          Message message;

          message.read(document.object());
          Q_EMIT self_->message (message);
        }
        catch (std::exception const & e)
        {
          Q_EMIT self_->error (QString {"MessageClient exception: %1"}.arg(e.what()));
        }
        catch (...)
        {
          Q_EMIT self_->error ("Unexpected exception in MessageClient");
        }
      }
    }
  }

  // If the message isn't exactly the same as the last one sent, emit it
  // as a datagram and note it as the last message sent.
  //
  // Caller is required to make the determination that our port and host
  // are valid prior to calling this function.

  void
  emit_message(QByteArray const & message)
  {
    if (message != lastMessage_)
    {
      writeDatagram(message, host_, port_);
      lastMessage_ = message;
    }
  }

  // If we've got a port, i.e., we're supposed to send messages, then queue
  // the message for later transmission if we don't have a host yet; attempt
  // to send it immediately if we've got a host.
  //
  // The message will be dropped on the floor if we don't have a port defined
  // or the message duplicates the last one sent.

  void
  send_message(QByteArray const & message)
  {
    if (port_)
    {
      if (host_.isNull()) messageQueue_.enqueue(message);
      else                emit_message(message);
    }
  }

  // Start a DNS lookup for the provided server name, noting that we have a
  // lookup in flight. If everything works out, and the host isn't blocked,
  // set our host to the first host address associated with the server and
  // send a ping.
  //
  // No matter the result of the host lookup, we're going to drain the queue,
  // either via sending messages if the host lookup worked, or by clearing it
  // if the lookup failed or the host is blocked.

  void
  queue_server_lookup(QString const & server)
  {
    hostLookupId_ = QHostInfo::lookupHost(server,
                                          this,
                                          [this](QHostInfo const & info)
    {
      if (info.lookupId() == hostLookupId_)
      {
        hostLookupId_ = -1;

        if (auto const & list = info.addresses();
                        !list.isEmpty())
        {
          auto const & host = list.at(0);

          if (!hostsBlocked_.contains(host))
          {
            host_ = host;

            ping();

            while (messageQueue_.size())
            {
              send_message(messageQueue_.dequeue());
            }
          }
          else
          {
            Q_EMIT self_->error ("UDP server blocked, please try another");
            messageQueue_.clear();
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

  MessageClient    * self_;
  port_type          port_;
  QTimer           * ping_;
  QHostAddress       host_;
  int                hostLookupId_ = -1;
  QSet<QHostAddress> hostsBlocked_;
  QQueue<QByteArray> messageQueue_;
  QByteArray         lastMessage_;
};

/******************************************************************************/
// Implementation
/******************************************************************************/

#include "MessageClient.moc"

MessageClient::MessageClient(QString   const & server,
                             port_type const   server_port,
                             QObject         * self)
  : QObject {self}
  , m_      {server_port, this}
{
  connect(&*m_, &impl::errorOccurred, [this](impl::SocketError e)
  {
#if defined (Q_OS_WIN) // Remove this when Qt stops doing this spuriously.
    if (e != impl::NetworkError &&
        e != impl::ConnectionRefusedError)
#else
    Q_UNUSED (e);
#endif
    {
      Q_EMIT error (m_->errorString());
    }
  });

  set_server(server);
}

QHostAddress
MessageClient::server_address() const
{
  return m_->host_;
}

MessageClient::port_type
MessageClient::server_port() const
{
  return m_->port_;
}

void
MessageClient::set_server(QString const & server)
{
  qDebug() << "server changed to" << server;

  m_->host_.clear();

  if (!server.isEmpty()) m_->queue_server_lookup(server);
}

void
MessageClient::set_server_port(port_type const port)
{
  m_->port_ = port;
}

void
MessageClient::send(Message const & message)
{
  m_->send_message(message.toJson());
}

void
MessageClient::send_raw_datagram(QByteArray   const & message,
                                 QHostAddress const & dest_address,
                                 port_type    const   dest_port)
{
  if (dest_port && !dest_address.isNull())
  {
    m_->writeDatagram(message, dest_address, dest_port);
  }
}

void
MessageClient::add_blocked_destination(QHostAddress const & host)
{
  m_->hostsBlocked_.insert(host);

  if (host == m_->host_)
  {
    m_->host_.clear();
    Q_EMIT error ("UDP server blocked, please try another");
    m_->messageQueue_.clear();
  }
}

/******************************************************************************/
