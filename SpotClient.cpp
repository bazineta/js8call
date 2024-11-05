#include "SpotClient.h"
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QQueue>
#include <QTimer>
#include <QUdpSocket>
#include "Message.hpp"
#include "pimpl_impl.hpp"
#include "moc_SpotClient.cpp"

/******************************************************************************/
// Constants
/******************************************************************************/

namespace
{
  constexpr auto SEND_INTERVAL = std::chrono::seconds(60);
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

class SpotClient::impl final : public QUdpSocket
{
  Q_OBJECT

public:

  // Constructor

  explicit impl(quint16 const port,
                SpotClient  * self)
    : self_ {self}
    , port_ {port}
    , send_ {new QTimer {this}}
  {
    connect(send_, &QTimer::timeout, this, [this]()
    {
      if (!host_.isNull())
      {
        while (!queue_.isEmpty())
        {
          writeDatagram(queue_.dequeue().toJson(), host_, port_);
        }
      }

      sent_++;
    });

    send_->start(SEND_INTERVAL);
  }

  // Destructor

  ~impl()
  {
    abort_host_lookup();
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
  // first address associated with the server as our host address.

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

          qDebug() << "SpotClient Host:" << host_.toString();;

          if (state() != UnconnectedState) close();
          
          bind(host_.protocol() == IPv6Protocol ? QHostAddress::AnyIPv6
                                                : QHostAddress::AnyIPv4);
        }
        else
        {
          Q_EMIT self_->error (QString {"Host lookup failed: %1"}.arg(info.errorString()));
        }
      }
    });
  }

  // Data members

  SpotClient    * self_;
  quint16         port_;
  QTimer        * send_;
  QHostAddress    host_;
  int             hostLookupId_ = -1;
  int             sent_         =  0;
  QString         call_;
  QString         grid_;
  QString         info_;
  QString         version_;
  QQueue<Message> queue_;
};


/******************************************************************************/
// Implementation
/******************************************************************************/

#include "SpotClient.moc"

// Constructor
//
// On Windows, Qt will seemingly emit spurious 'connection refused' errors
// for UDP sockets; ignore them if they appear.

SpotClient::SpotClient(QString const & name,
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

  m_->queue_host_lookup(name);
}

void
SpotClient::setLocalStation(QString const & callsign,
                            QString const & grid,
                            QString const & info,
                            QString const & version)
{
  auto const valueChanged = [](QString       & oldValue,
                               QString const & newValue)
  {
    if (oldValue == newValue) return false;
    oldValue = newValue;
    return true;
  };

  bool const changed = valueChanged(m_->call_,    callsign)
                     | valueChanged(m_->grid_,    grid)
                     | valueChanged(m_->info_,    info)
                     | valueChanged(m_->version_, version);

  // Send local information to network on change, or once every 15 minutes.

  if (changed || m_->sent_ % 15 == 0)
  {
    m_->queue_.enqueue({"RX.LOCAL", "", {
      {"CALLSIGN", QVariant(callsign)},
      {"GRID",     QVariant(grid)    },
      {"INFO",     QVariant(info)    },
      {"VERSION",  QVariant(version) }
    }});
  }
}

void
SpotClient::enqueueCmd(QString const & cmd,
                       QString const & from,
                       QString const & to,
                       QString const & relayPath,
                       QString const & text,
                       QString const & grid,
                       QString const & extra,
                       int     const   submode,
                       int     const   dial,
                       int     const   offset,
                       int     const   snr)
{
  m_->queue_.enqueue({"RX.DIRECTED", "", {
    {"BY", QVariant(QVariantMap {
      {"CALLSIGN", QVariant(m_->call_)},
      {"GRID",     QVariant(m_->grid_)},
    })},
    {"CMD",    QVariant(cmd)          },
    {"FROM",   QVariant(from)         },
    {"TO",     QVariant(to)           },
    {"PATH",   QVariant(relayPath)    },
    {"TEXT",   QVariant(text)         },
    {"GRID",   QVariant(grid)         },
    {"EXTRA",  QVariant(extra)        },
    {"FREQ",   QVariant(dial + offset)},
    {"DIAL",   QVariant(dial)         },
    {"OFFSET", QVariant(offset)       },
    {"SNR",    QVariant(snr)          },
    {"SPEED",  QVariant(submode)      }
  }});
}

void
SpotClient::enqueueSpot(QString const & callsign,
                        QString const & grid,
                        int     const   submode,
                        int     const   dial,
                        int     const   offset,
                        int     const   snr)
{
  m_->queue_.enqueue({"RX.SPOT", "", {
    {"BY", QVariant(QVariantMap {
      {"CALLSIGN", QVariant(m_->call_)},
      {"GRID",     QVariant(m_->grid_)},
    })},
    {"CALLSIGN", QVariant(callsign)     },
    {"GRID",     QVariant(grid)         },
    {"FREQ",     QVariant(dial + offset)},
    {"DIAL",     QVariant(dial)         },
    {"OFFSET",   QVariant(offset)       },
    {"SNR",      QVariant(snr)          },
    {"SPEED",    QVariant(submode)      }
  }});
}

/******************************************************************************/
