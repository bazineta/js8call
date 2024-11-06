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
// Local Utilities
/******************************************************************************/

namespace
{
  template <typename T>
  bool
  changeValue(T       & stored,
              T const & update)
  {
    if (stored == update) return false;
        stored =  update; return true;
  }
}

/******************************************************************************/
// Private Implementation
/******************************************************************************/

class SpotClient::impl final : public QUdpSocket
{
  Q_OBJECT

public:

  // Constructor

  impl(QString const & name,
       quint16 const   port,
       SpotClient    * self)
    : self_ {self}
    , port_ {port}
    , send_ {new QTimer {this}}
  {
    hostLookupId_ = QHostInfo::lookupHost(name,
                                          this,
                                          [this](QHostInfo const & info)
    {
      hostLookupId_ = -1;

      if (auto const & list = info.addresses();
                      !list.isEmpty())
      {
        host_ = list.first();

        qDebug() << "SpotClient Host:" << host_.toString();
        
        bind(host_.protocol() == IPv6Protocol ? QHostAddress::AnyIPv6
                                              : QHostAddress::AnyIPv4);
      }
      else
      {
        Q_EMIT self_->error (QString {"Host lookup failed: %1"}.arg(info.errorString()));
        valid_ = false;
        queue_.clear();
      }
    });

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
    if (hostLookupId_ != -1) QHostInfo::abortHostLookup(hostLookupId_);
  }

  // Data members

  SpotClient    * self_;
  quint16         port_;
  QTimer        * send_;
  QHostAddress    host_;
  bool            valid_        =  true;
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
  , m_      {name, port, this}
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
}

void
SpotClient::setLocalStation(QString const & callsign,
                            QString const & grid,
                            QString const & info,
                            QString const & version)
{
  auto const changed = changeValue(m_->call_,    callsign) +
                       changeValue(m_->grid_,    grid)     +
                       changeValue(m_->info_,    info)     +
                       changeValue(m_->version_, version);

  // Send local information to network on change, or once every 15 minutes.

  if (m_->valid_ && (changed || m_->sent_ % 15 == 0))
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
  if (m_->valid_)
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
}

void
SpotClient::enqueueSpot(QString const & callsign,
                        QString const & grid,
                        int     const   submode,
                        int     const   dial,
                        int     const   offset,
                        int     const   snr)
{
  if (m_->valid_)
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
}

/******************************************************************************/
