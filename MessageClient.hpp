#ifndef MESSAGE_CLIENT_HPP__
#define MESSAGE_CLIENT_HPP__

#include <QDataStream>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTime>

#include "Message.h"
#include "Radio.hpp"
#include "pimpl_h.hpp"

class QByteArray;
class QHostAddress;

//
// MessageClient - Manage messages sent and replies received from a
//                 matching server (MessageServer) at the other end of
//                 the wire
//
//
// Each outgoing message type is a Qt slot
//
class MessageClient
  : public QObject
{
  Q_OBJECT

public:
  using Frequency = Radio::Frequency;
  using port_type = quint16;

  // instantiate and initiate a host lookup on the server
  //
  // messages will be queued until a server host lookup is complete
  MessageClient (QString const& server, port_type server_port, QObject * parent = nullptr);

  // query server details
  QHostAddress server_address () const;
  port_type server_port () const;

  // initiate a new server host lookup or is the server name is empty
  // the sending of messages is disabled
  Q_SLOT void set_server (QString const& server = QString {});

  // change the server port messages are sent to
  Q_SLOT void set_server_port (port_type server_port = 0u);

  // this slot is used to send an arbitrary message
  Q_SLOT void send(Message const &message);

  // this slot may be used to send arbitrary UDP datagrams to and
  // destination allowing the underlying socket to be used for general
  // UDP messaging if desired
  Q_SLOT void send_raw_datagram (QByteArray const&, QHostAddress const& dest_address, port_type dest_port);

  // disallowed message destination (does not block datagrams sent
  // with send_raw_datagram() above)
  Q_SLOT void add_blocked_destination (QHostAddress const&);

  Q_SIGNAL void message(Message const &message);

  // this signal is emitted when the a reply a message is received
  Q_SIGNAL void message_received(QString const &type, QString const &message);

  // this signal is emitted when network errors occur or if a host
  // lookup fails
  Q_SIGNAL void error (QString const&) const;

private:
  class impl;
  pimpl<impl> m_;
};

#endif
