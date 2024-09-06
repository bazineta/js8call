#include "PSKReporter.hpp"

// Interface for posting spots to PSK Reporter web site
// Implemented by Edson Pereira PY2SDR
// Updated by Bill Somerville, G4WJS
//
// Reports will be sent in batch mode every 5 minutes.

#include <fstream>
#include <iostream>
#include <cmath>
#include <ctime>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QHostInfo>
#include <QObject>
#include <QQueue>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <QString>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

#include "Configuration.hpp"
#include "pimpl_impl.hpp"

#include "DriftingDateTime.h"

#include "moc_PSKReporter.cpp"

#define DEBUGECLIPSE 0

namespace
{
  using namespace Qt::Literals::StringLiterals;

  const     auto             HOST               = u"report.pskreporter.info"_s;
  constexpr quint16          SERVICE_PORT       = 4739;
  constexpr int              MIN_SEND_INTERVAL  = 120;                   // in seconds
  constexpr int              FLUSH_INTERVAL     = MIN_SEND_INTERVAL + 5; // in send intervals
  constexpr bool             ALIGNMENT_PADDING  = true;
  constexpr int              MIN_PAYLOAD_LENGTH = 508;
  constexpr int              MAX_PAYLOAD_LENGTH = 10000;
  constexpr std::time_t      CACHE_TIMEOUT      = 300;                  // default to 5 minutes for repeating spots
  constexpr Radio::Frequency CACHE_EXEMPT_FREQ  = 49000000;
}

class PSKReporter::impl final
  : public QObject
{
  Q_OBJECT

public:
  impl (PSKReporter         * self,
        Configuration const * config,
        QString       const & program_info)
    : self_    {self}
    , config_  {config}
    , prog_id_ {program_info}
  {
    // This timer sets the interval to check for spots to send.
    connect (&report_timer_, &QTimer::timeout, [this] () {send_report ();});

    // This timer repeats the sending of IPFIX templates and receiver
    // information if we are using UDP, in case server has been
    // restarted ans lost cached information.
    connect (&descriptor_timer_, &QTimer::timeout, [this] () {
                                                     if (socket_
                                                         && QAbstractSocket::UdpSocket == socket_->socketType ())
                                                       {
                                                         // qDebug() << "[PSK]enable descriptor resend";
                                                         // send templates and receiver data set again,
                                                         // 3 times.
                                                         send_descriptors_   = 3;
                                                         send_receiver_data_ = 3;
                                                       }
                                                   });
    eclipse_load(config->data_dir ().absoluteFilePath ("eclipse.txt"));
  }

  void check_connection ()
  {
    if (!socket_
        || QAbstractSocket::UnconnectedState == socket_->state ()
        || (socket_->socketType () != (config_->psk_reporter_tcpip () ? QAbstractSocket::TcpSocket : QAbstractSocket::UdpSocket)))
      {
        // we need to create the appropriate socket
        if (socket_
            && QAbstractSocket::UnconnectedState != socket_->state ()
            && QAbstractSocket::ClosingState     != socket_->state ())
          {
            // qDebug() << "[PSK]create/recreate socket";
            // handle re-opening asynchronously
            auto connection = QSharedPointer<QMetaObject::Connection>::create ();
            *connection = connect (socket_.data (), &QAbstractSocket::disconnected, [this, connection] () {
                                                                                     disconnect (*connection);
                                                                                     check_connection ();
                                                                                   });
            // close gracefully
            send_report (true);
            socket_->close ();
          }
        else
          {
            reconnect ();
          }
      }
  }

  void handle_socket_error (QAbstractSocket::SocketError e)
  {
    qWarning() << "[PSK]socket error:" << socket_->errorString ();
    switch (e)
      {
      case QAbstractSocket::RemoteHostClosedError:
        socket_->disconnectFromHost ();
        break;

      case QAbstractSocket::TemporaryError:
        break;

      default:
        spots_.clear ();
        Q_EMIT self_->errorOccurred (socket_->errorString ());
        break;
      }
  }

  void reconnect ()
  {
    // Using deleteLater for the deleter as we may eventually
    // be called from the disconnected handler above.
    if (config_->psk_reporter_tcpip ())
      {
        // qDebug() << "[PSK]create TCP/IP socket";
        socket_.reset (new QTcpSocket, &QObject::deleteLater);
        send_descriptors_   = 1;
        send_receiver_data_ = 1;
      }
    else
      {
        // qDebug() << "[PSK]create UDP/IP socket";
        socket_.reset (new QUdpSocket, &QObject::deleteLater);
        send_descriptors_   = 3;
        send_receiver_data_ = 3;
      }

    connect(socket_.get(),
            &QAbstractSocket::errorOccurred,
            this,
            &PSKReporter::impl::handle_socket_error);

    // use this for pseudo connection with UDP, allows us to use
    // QIODevice::write() instead of QUDPSocket::writeDatagram()
    socket_->connectToHost (HOST, SERVICE_PORT, QAbstractSocket::WriteOnly);
    qDebug() << "[PSK]remote host:" << HOST << "port:" << SERVICE_PORT;

    if (!report_timer_.isActive ())
      {
        report_timer_.start (MIN_SEND_INTERVAL+1 * 1000); // we add 1 to give some more randomization
      }
    if (!descriptor_timer_.isActive ())
      {
        descriptor_timer_.start (1 * 60 * 60 * 1000); // hourly
      }
  }

  void stop ()
  {
    if (socket_)
      {
        // qDebug() << "[PSK]disconnecting";
        socket_->disconnectFromHost ();
      }
    descriptor_timer_.stop ();
    report_timer_.stop ();
  }

  void send_report (bool send_residue = false);
  void build_preamble (QDataStream&);
  void eclipse_load(QString filename);
  bool eclipse_active(QDateTime) const;

  bool flushing ()
  {
    bool flush =  FLUSH_INTERVAL && !(++flush_counter_ % FLUSH_INTERVAL);
    // qDebug() <<  "[PSK]flush: " << flush;
    return flush;
  }

  QList<QDateTime> eclipseDates;

  PSKReporter * self_;
  Configuration const * config_;
  QSharedPointer<QAbstractSocket> socket_;
  int dns_lookup_id_;
  QByteArray payload_;
  quint32 sequence_number_  = 0u;
  int     send_descriptors_ = 0;

  // Currently PSK Reporter requires that  a receiver data set is sent
  // in every  data flow. This  memeber variable  can be used  to only
  // send that information at session start (3 times for UDP), when it
  // changes (3  times for UDP), or  once per hour (3  times) if using
  // UDP. Uncomment the relevant code to enable that fuctionality.

  int send_receiver_data_ = 0;
  unsigned flush_counter_ = 0u;
  quint32 observation_id_ = QRandomGenerator::global()->generate();
  QString rx_call_;
  QString rx_grid_;
  QString rx_ant_;
  QString prog_id_;
  QByteArray tx_data_;
  QByteArray tx_residue_;
  struct Spot
  {
    bool operator == (Spot const& rhs) const
    {
      return
        call_ == rhs.call_
        && grid_ == rhs.grid_
        && mode_ == rhs.mode_
        && std::abs (Radio::FrequencyDelta (freq_ - rhs.freq_)) < 50;
    }

    QString call_;
    QString grid_;
    int snr_;
    Radio::Frequency freq_;
    QString mode_;
    QDateTime time_;
  };
  QQueue<Spot> spots_;
  QHash<QString, std::time_t> spot_cache_;
  QTimer report_timer_;
  QTimer descriptor_timer_;
};
  
#include "PSKReporter.moc"

namespace
{
  void
  writeUtfString(QDataStream   & out,
                 QString const & s)
  {
    auto const& utf = s.toUtf8 ().left (254);
    out << quint8 (utf.size ());
    out.writeRawData (utf, utf.size ());
  }

  qsizetype
  num_pad_bytes(qsizetype const len)
  {
    return ALIGNMENT_PADDING ? (4 - len % 4) % 4 : 0;
  }

  void
  set_length(QDataStream       & out,
             QByteArray  const & b)
  {
    // pad with nulls modulo 4
    auto const pad_len = num_pad_bytes (b.size ());
    out.writeRawData (QByteArray {pad_len, '\0'}.constData (), pad_len);
    auto pos = out.device ()->pos ();
    out.device ()->seek (sizeof (quint16));
    // insert length
    out << static_cast<quint16> (b.size ());
    out.device ()->seek (pos);
  }
}

bool
PSKReporter::impl::eclipse_active(QDateTime const dateNow) const
{
  return std::any_of(eclipseDates.begin(),
                     eclipseDates.end(),
                     [=](auto const check)
  {
    // +- 6 hour window
    return qAbs(check.secsTo(dateNow)) <= (3600 * 6); // 6 hour check
  });
}

void PSKReporter::impl::eclipse_load(QString eclipse_file)
{
    std::ifstream fs(qPrintable(eclipse_file));
    std::string mydate;
    std::string mytime;
    std::string myline;
#if DEBUGECLIPSE
    std::ofstream mylog("c:/temp/eclipse.log");
    mylog << "eclipse_file=" << eclipse_file << std::endl;
#endif
    if (fs.is_open())
    {
          while(!fs.eof())
          {
              std::getline(fs, myline);
              if (myline[0] != '#' && myline.length() > 2)  // make sure to skip blank lines
              {
                //QString format = "yyyy-MM-dd hh:mm:ss";
                QDateTime qdate = QDateTime::fromString(QString::fromStdString(myline), Qt::ISODate);
                QDateTime now = DriftingDateTime::currentDateTimeUtc();
                // only add the date if we can cover the whole 12 hours
                //if (now < qdate.toUTC().addSecs(-3600*6))
                eclipseDates.append(qdate);
#if DEBUGECLIPSE
				//else
				//  mylog << "not adding " << myline << std::endl;
#endif
	
              }
#if DEBUGECLIPSE
              mylog << myline << std::endl;
#endif
          }
    }
#if DEBUGECLIPSE
    // if (eclipse_active(QDateTime::currentDateTime().toUTC())) mylog << "Eclipse is active" << std::endl;
    if (eclipse_active(DriftingDateTime::currentDateTime().toUTC())) mylog << "Eclipse is active" << std::endl;
    else mylog << "Eclipse is not active" << std::endl;
#endif
}

void PSKReporter::impl::build_preamble (QDataStream& message)
{
  // Message Header
  message
    << quint16 (10u)          // Version Number
    << quint16 (0u)           // Length (place-holder filled in later)
    << quint32 (0u)           // Export Time (place-holder filled in later)
    << ++sequence_number_     // Sequence Number
    << observation_id_;       // Observation Domain ID
  // qDebug() << "[PSK]#:" << sequence_number_;

  if (send_descriptors_)
    {
      --send_descriptors_;
      {
        // Sender Information descriptor
        QByteArray descriptor;
        QDataStream out {&descriptor, QIODevice::WriteOnly};
        out
          << quint16 (2u)           // Template Set ID
          << quint16 (0u)           // Length (place-holder)
          << quint16 (0x50e3)       // Link ID
          << quint16 (7u)           // Field Count
          << quint16 (0x8000 + 1u)  // Option 1 Information Element ID (senderCallsign)
          << quint16 (0xffff)       // Option 1 Field Length (variable)
          << quint32 (30351u)       // Option 1 Enterprise Number
          << quint16 (0x8000 + 5u)  // Option 2 Information Element ID (frequency)
          << quint16 (5u)           // Option 2 Field Length
          << quint32 (30351u)       // Option 2 Enterprise Number
          << quint16 (0x8000 + 6u)  // Option 3 Information Element ID (sNR)
          << quint16 (1u)           // Option 3 Field Length
          << quint32 (30351u)       // Option 3 Enterprise Number
          << quint16 (0x8000 + 10u) // Option 4 Information Element ID (mode)
          << quint16 (0xffff)       // Option 4 Field Length (variable)
          << quint32 (30351u)       // Option 4 Enterprise Number
          << quint16 (0x8000 + 3u)  // Option 5 Information Element ID (senderLocator)
          << quint16 (0xffff)       // Option 5 Field Length (variable)
          << quint32 (30351u)       // Option 5 Enterprise Number
          << quint16 (0x8000 + 11u) // Option 6 Information Element ID (informationSource)
          << quint16 (1u)           // Option 6 Field Length
          << quint32 (30351u)       // Option 6 Enterprise Number
          << quint16 (150u)         // Option 7 Information Element ID (dateTimeSeconds)
          << quint16 (4u);          // Option 7 Field Length
        // insert Length and move to payload
        set_length (out, descriptor);
        message.writeRawData (descriptor.constData (), descriptor.size ());
      }
      {
        // Receiver Information descriptor
        QByteArray descriptor;
        QDataStream out {&descriptor, QIODevice::WriteOnly};
        out
          << quint16 (3u)          // Options Template Set ID
          << quint16 (0u)          // Length (place-holder)
          << quint16 (0x50e2)      // Link ID
          << quint16 (4u)          // Field Count
          << quint16 (0u)          // Scope Field Count
          << quint16 (0x8000 + 2u) // Option 1 Information Element ID (receiverCallsign)
          << quint16 (0xffff)      // Option 1 Field Length (variable)
          << quint32 (30351u)      // Option 1 Enterprise Number
          << quint16 (0x8000 + 4u) // Option 2 Information Element ID (receiverLocator)
          << quint16 (0xffff)      // Option 2 Field Length (variable)
          << quint32 (30351u)      // Option 2 Enterprise Number
          << quint16 (0x8000 + 8u) // Option 3 Information Element ID (decodingSoftware)
          << quint16 (0xffff)      // Option 3 Field Length (variable)
          << quint32 (30351u)      // Option 3 Enterprise Number
          << quint16 (0x8000 + 9u) // Option 4 Information Element ID (antennaInformation)
          << quint16 (0xffff)      // Option 4 Field Length (variable)
          << quint32 (30351u);     // Option 4 Enterprise Number
        // insert Length
        set_length (out, descriptor);
        message.writeRawData (descriptor.constData (), descriptor.size ());
        qDebug() << "[PSK]sent descriptors";
      }
    }

  // if (send_receiver_data_)
  {
    // --send_receiver_data_;

    // Receiver information
    QByteArray data;
    QDataStream out {&data, QIODevice::WriteOnly};

    // Set Header
    out
      << quint16 (0x50e2)     // Template ID
      << quint16 (0u);        // Length (place-holder)

    // Set data
    writeUtfString (out, rx_call_);
    writeUtfString (out, rx_grid_);
    writeUtfString (out, prog_id_);
    writeUtfString (out, rx_ant_);

    // insert Length and move to payload
    set_length (out, data);
    message.writeRawData (data.constData (), data.size ());
    qDebug() << "[PSK]sent local information";
  }
}

void PSKReporter::impl::send_report (bool send_residue)
{
  // qDebug() << "[PSK]sending residue:" << send_residue;
  if (QAbstractSocket::ConnectedState != socket_->state ()) return;

  QDataStream message {&payload_, QIODevice::WriteOnly | QIODevice::Append};
  QDataStream tx_out {&tx_data_, QIODevice::WriteOnly | QIODevice::Append};

  if (!payload_.size ())
    {
      // Build header, optional descriptors, and receiver information
      build_preamble (message);
    }

  auto flush = flushing () || send_residue;
  while (spots_.size () || flush)
    {
      if (!payload_.size ())
        {
          // Build header, optional descriptors, and receiver information
          build_preamble (message);
        }

      if (!tx_data_.size () && (spots_.size () || tx_residue_.size ()))
        {
          // Set Header
          tx_out
            << quint16 (0x50e3)     // Template ID
            << quint16 (0u);        // Length (place-holder)
        }

      // insert any residue
      if (tx_residue_.size ())
        {
          tx_out.writeRawData (tx_residue_.constData (), tx_residue_.size ());
          // qDebug() << "[PSK]sent residue";
          tx_residue_.clear ();
        }

      qDebug() << "[PSK]pending spots:" << spots_.size ();
      while (spots_.size () || flush)
        {
          auto tx_data_size = tx_data_.size ();
          if (spots_.size ())
            {
              auto const& spot = spots_.dequeue ();

              // Sender information
              writeUtfString (tx_out, spot.call_);
              uint8_t data[5];
              long long int i64 = spot.freq_;
              data[0] = ( i64 & 0xff);
              data[1] = ((i64 >>  8) & 0xff);
              data[2] = ((i64 >> 16) & 0xff);
              data[3] = ((i64 >> 24) & 0xff);
              data[4] = ((i64 >> 32) & 0xff);
              tx_out // BigEndian
                << data[4]
                << data[3]
                << data[2]
                << data[1]
                << data[0]
                << static_cast<qint8> (spot.snr_);
              writeUtfString (tx_out, spot.mode_);
              writeUtfString (tx_out, spot.grid_);
              tx_out
                << quint8 (1u)          // REPORTER_SOURCE_AUTOMATIC
                << static_cast<quint32> (spot.time_.toSecsSinceEpoch());
            }

          auto len = payload_.size () + tx_data_.size ();
          len += num_pad_bytes (tx_data_.size ());
          len += num_pad_bytes (len);
          if (len > MAX_PAYLOAD_LENGTH // our upper datagram size limit
              || (!spots_.size () && len > MIN_PAYLOAD_LENGTH) // spots drained and above lower datagram size limit
              || (flush && !spots_.size ())) // send what we have, possibly no spots
            {
              if (tx_data_.size ())
                {
                  if (len <= MAX_PAYLOAD_LENGTH)
                    {
                      tx_data_size = tx_data_.size ();
                    }
                  QByteArray tx {tx_data_.left (tx_data_size)};
                  QDataStream out {&tx, QIODevice::WriteOnly | QIODevice::Append};
                  // insert Length
                  set_length (out, tx);
                  message.writeRawData (tx.constData (), tx.size ());
                }

              // insert Length and Export Time
              set_length (message, payload_);
              message.device ()->seek (2 * sizeof (quint16));
              message << static_cast<quint32> (
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
                                               DriftingDateTime::currentDateTime ().toSecsSinceEpoch ()
#else
                                               DriftingDateTime::currentDateTime ().toMSecsSinceEpoch () / 1000
#endif
                                               );

              // Send data to PSK Reporter site
              socket_->write (payload_); // TODO: handle errors
              qDebug() << "[PSK]sent spots";
              flush = false;    // break loop
              message.device ()->seek (0u);
              payload_.clear ();  // Fresh message
              // Save unsent spots
              tx_residue_ = tx_data_.right (tx_data_.size () - tx_data_size);
              tx_out.device ()->seek (0u);
              tx_data_.clear ();
              break;
            }
        }
      qDebug() << "[PSK]remaining spots:" << spots_.size ();
    }
}

PSKReporter::PSKReporter(Configuration const * config,
                         QString        const& program_info)
  : m_ {this, config, program_info}
{
  qDebug() << "[PSK]Started for: " << program_info;
}

PSKReporter::~PSKReporter()
{
  // m_->send_report (true);       // send any pending spots
  qDebug() << "[PSK]Ended";
}

void PSKReporter::reconnect()
{
  m_->reconnect();
}

bool
PSKReporter::eclipse_active(QDateTime const now) const
{
  return m_->eclipse_active(now);
}

void
PSKReporter::setLocalStation(QString const & call,
                             QString const & gridSquare,
                             QString const & antenna)
{
  m_->check_connection();

  if (call       != m_->rx_call_ ||
      gridSquare != m_->rx_grid_ ||
      antenna    != m_->rx_ant_)
  {
    qDebug() << "[PSK]updating information";
    m_->send_receiver_data_ = m_->socket_ && QAbstractSocket::UdpSocket == m_->socket_->socketType() ? 3 : 1;
    m_->rx_call_            = call;
    m_->rx_grid_            = gridSquare;
    m_->rx_ant_             = antenna;
  }
}

bool
PSKReporter::addRemoteStation(QString   const & call,
                               QString  const & grid,
                               Radio::Frequency freq,
                               QString  const & mode,
                               int snr)
{
  m_->check_connection();

  if (!m_->socket_)            return false;
  if (!m_->socket_->isValid()) return false;

  if (QAbstractSocket::UnconnectedState == m_->socket_->state())
  {
    reconnect();
  }

  // If this call is not already in the cache, or it's there but expired, or the
  // frequency is interesting, or an eclipse is active, (we allow all spots through
  // +/- 6 hours around an eclipse for the HamSCI group) then we're going to send
  // the spot; cache the fact that we've done so, either by adding a new cache
  // entry or updating an existing one with an updated time value.

  if (auto const it  = m_->spot_cache_.find(call);
                 it == m_->spot_cache_.end()    ||
                 it.value() > CACHE_TIMEOUT     ||
                 freq       > CACHE_EXEMPT_FREQ ||
                 eclipse_active(DriftingDateTime::currentDateTime().toUTC()))
  {
    m_->spots_.enqueue({call, grid, snr, freq, mode, DriftingDateTime::currentDateTimeUtc()});
    m_->spot_cache_.insert(call, std::time(nullptr));
  }

  // Perform cache cleanup; anything that's been around for twice the cache timeout
  // period can go.

  m_->spot_cache_.removeIf([now = std::time(nullptr)](auto const it)
  {
    return now - it.value() > (CACHE_TIMEOUT * 2);
  });

  return true;
}

void PSKReporter::sendReport(const bool last)
{
  m_->check_connection();

  if (m_->socket_ && QAbstractSocket::ConnectedState == m_->socket_->state())
  {
    m_->send_report(true);
  }

  if (last)
  {
    m_->stop();
  }
}
