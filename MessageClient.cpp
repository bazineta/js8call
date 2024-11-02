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
  constexpr auto HEARTBEAT_MS = 15 * 1000;
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

class MessageClient::impl final : public QUdpSocket
{
  Q_OBJECT

public:

  // Constructor

  impl(QString   const & id,
       QString   const & version,
       QString   const & revision,
       port_type const   server_port,
       MessageClient   * self)
    : self_            {self}
    , id_              {id}
    , version_         {version}
    , revision_        {revision}
    , server_port_     {server_port}
    , heartbeat_timer_ {new QTimer {this}}
  {
    connect (heartbeat_timer_, &QTimer::timeout,      this, &impl::heartbeat);
    connect (this,             &QIODevice::readyRead, this, &impl::pending_datagrams);

    heartbeat_timer_->start(HEARTBEAT_MS);

    bind();
  }

  // Destructor

  ~impl ()
  {
    if (server_port_ && !server_.isNull())
    {
      Message m("CLOSE");
      writeDatagram(m.toJson(), server_, server_port_);
    }

    if (dns_lookup_id_ != -1)
    {
      QHostInfo::abortHostLookup(dns_lookup_id_);
    }
  }

  enum class StreamStatus
  {
    Fail,
    Short,
    OK
  };

  // Accesssors

  StreamStatus
  check_status(QDataStream const & stream) const
  {
    switch (stream.status())
    {
      case QDataStream::ReadPastEnd:
        return StreamStatus::Short;

      case QDataStream::ReadCorruptData:
        Q_EMIT self_->error ("Message serialization error: read corrupt data");
        return StreamStatus::Fail;

      case QDataStream::WriteFailed:
        Q_EMIT self_->error ("Message serialization error: write error");
        return StreamStatus::Fail;

      default:
        return StreamStatus::OK;
    }
  }

  // Manipulators

  void
  parse_message(QByteArray const & msg)
  {
    try
    {
      if (msg.isEmpty())return;

      QJsonParseError e;
      QJsonDocument   d = QJsonDocument::fromJson(msg, &e);

      if (e.error != QJsonParseError::NoError)
      {
        Q_EMIT self_->error(QString {"MessageClient json parse error:  %1"}.arg(e.errorString()));
        return;
      }

      if (!d.isObject())
      {
        Q_EMIT self_->error(QString {"MessageClient json parse error: json is not an object"});
        return;
      }

      Message m;

      m.read(d.object());
      Q_EMIT self_->message(m);
    }
    catch (std::exception const & e)
    {
      Q_EMIT self_->error (QString {"MessageClient exception: %1"}.arg (e.what ()));
    }
    catch (...)
    {
      Q_EMIT self_->error ("Unexpected exception in MessageClient");
    }
  }

  void
  pending_datagrams()
  {
    while (hasPendingDatagrams())
    {
      QByteArray   datagram;
      QHostAddress sender_address;
      port_type    sender_port;

      datagram.resize(pendingDatagramSize());
      
      if (0 <= readDatagram(datagram.data(),
                            datagram.size(),
                            &sender_address,
                            &sender_port))
      {
        parse_message(datagram);
      }
    }
  }

  void
  heartbeat()
  {
    if (server_port_ && !server_.isNull())
    {
      Message m("PING", "", QMap<QString, QVariant>{
        {"NAME",    QVariant(QApplication::applicationName())},
        {"VERSION", QVariant(QApplication::applicationVersion())},
        {"UTC",     QVariant(DriftingDateTime::currentDateTimeUtc().toMSecsSinceEpoch())}
      });
      writeDatagram(m.toJson(), server_, server_port_);
    }
  }

  void
  send_message(QByteArray const & message)
  {
    if (server_port_)
    {
      if (!server_.isNull())
      {
        if (message != last_message_) // avoid duplicates
        {
          writeDatagram(message, server_, server_port_);
          last_message_ = message;
        }
      }
      else
      {
        pending_messages_.enqueue(message);
      }
    }
  }

  void
  send_message(QDataStream const & out,
                QByteArray const & message)
  {
    if (check_status(out) == StreamStatus::OK)
    {
      send_message(message);
    }
    else
    {
      Q_EMIT self_->error ("Error creating UDP message");
    }
  }

  void
  queue_server_lookup(QString const & server)
  {
    dns_lookup_id_ = QHostInfo::lookupHost(server,
                                           this,
                                           [this](QHostInfo const & info)
    {
      if (auto const & addresses = info.addresses();
                      !addresses.isEmpty())
      {
        auto const & server = addresses.at(0);

        if (!blocked_addresses_.contains(server))
        {
          server_ = server;

          // send initial heartbeat which allows schema negotiation
          heartbeat();

          // clear any backlog
          while (pending_messages_.size())
          {
            send_message(pending_messages_.dequeue());
          }
        }
        else
        {
          Q_EMIT self_->error ("UDP server blocked, please try another");
          pending_messages_.clear(); // discard
        }
      }
      else
      {
        Q_EMIT self_->error (QString("UDP server lookup failed: %1").arg(info.errorString()));
        pending_messages_.clear(); // discard
      }
    });
  }

  // Data members

  MessageClient    * self_;
  QString            id_;
  QString            version_;
  QString            revision_;
  QString            server_string_;
  port_type          server_port_;
  QHostAddress       server_;
  QTimer           * heartbeat_timer_;
  QSet<QHostAddress> blocked_addresses_;
  QQueue<QByteArray> pending_messages_;
  QByteArray         last_message_;
  int                dns_lookup_id_ = -1;
};

/******************************************************************************/
// Implementation
/******************************************************************************/

#include "MessageClient.moc"

MessageClient::MessageClient(QString   const & id,
                             QString   const & version,
                             QString   const & revision,
                             QString   const & server,
                             port_type const   server_port,
                             QObject         * self)
  : QObject {self}
  , m_      {id,
             version,
             revision,
             server_port,
             this}
{
  connect(&*m_, &impl::errorOccurred,[this](impl::SocketError e)
  {
#if defined (Q_OS_WIN) && QT_VERSION >= 0x050500
    if (e != impl::NetworkError // take this out when Qt 5.5
                                // stops doing this
                                // spuriously
        && e != impl::ConnectionRefusedError) // not
                                              // interested
                                              // in this with
                                              // UDP socket
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
  return m_->server_;
}

MessageClient::port_type
MessageClient::server_port() const
{
  return m_->server_port_;
}

void
MessageClient::set_server(QString const & server)
{
  qDebug() << "server changed to" << server;

  m_->server_.clear();
  m_->server_string_ = server;

  if (!server.isEmpty()) m_->queue_server_lookup(server);
}

void
MessageClient::set_server_port(port_type const server_port)
{
  m_->server_port_ = server_port;
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
MessageClient::add_blocked_destination(QHostAddress const & address)
{
  m_->blocked_addresses_.insert(address);
  if (address == m_->server_)
  {
    m_->server_.clear();
    Q_EMIT error ("UDP server blocked, please try another");
    m_->pending_messages_.clear(); // discard
  }
}

/******************************************************************************/
