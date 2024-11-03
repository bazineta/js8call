#include "MessageClient.hpp"
#include <QApplication>
#include <QByteArray>
#include <QHostAddress>
#include <QHostInfo>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>

#include "DriftingDateTime.h"
#include "MessageError.hpp"
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
// Message Parsing
/******************************************************************************/

namespace
{
  // Exception thrown on message parsing errors.

  struct parse_error : public std::system_error
  {
    using std::system_error::system_error;

    explicit parse_error(QJsonParseError const & parse)
    : parse_error(MessageError::Code::json_parsing_error,
                  parse.errorString().toStdString())
    {}
  };

  // Parse and return the provided datagram as a Message object; throw
  // if parsing failed.

  Message
  parse_message(QByteArray const & datagram)
  {
    using MessageError::Code;

    QJsonParseError parse;
    QJsonDocument   document = QJsonDocument::fromJson(datagram, &parse);

    if (parse.error)          throw parse_error(parse);
    if (!document.isObject()) throw parse_error(Code::json_not_an_object);

    Message message;

    message.read(document.object());

    return message;
  }
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
            Q_EMIT self_->message (parse_message(datagram));
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

  // If the message isn't exactly the same as the one sent prior to this
  // one, emit it as a datagram and note it as the prior message sent.
  //
  // Caller is required to make the determination that our port and host
  // are valid prior to calling this function.

  void
  emit_message(QByteArray const & message)
  {
    if (message != priorMessage_)
    {
      writeDatagram(message, host_, port_);
      priorMessage_ = message;
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
  // lookup in flight. If everything works out set our host to the first host
  // address associated with the server and send a ping.
  //
  // No matter the result of the host lookup, we're going to drain the queue,
  // either via sending messages if the host lookup worked, or by clearing it
  // if the lookup failed.

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
          host_ = list.at(0);

          ping();

          while (messageQueue_.size())
          {
            send_message(messageQueue_.dequeue());
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
  quint16            port_;
  QTimer           * ping_;
  QHostAddress       host_;
  int                hostLookupId_ = -1;
  QQueue<QByteArray> messageQueue_;
  QByteArray         priorMessage_;
};

/******************************************************************************/
// Implementation
/******************************************************************************/

#include "MessageClient.moc"

MessageClient::MessageClient(QString const & server,
                             quint16 const   port,
                             QObject       * parent)
  : QObject {parent}
  , m_      {port, this}
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

quint16
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
MessageClient::set_server_port(quint16 const port)
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
                                 QHostAddress const & address,
                                 quint16      const   port)
{
  if (port && !address.isNull())
  {
    m_->writeDatagram(message, address, port);
  }
}

/******************************************************************************/
