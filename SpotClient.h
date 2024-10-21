#ifndef JS8SPOTCLIENT_H
#define JS8SPOTCLIENT_H

#include "MessageClient.hpp"

#include <QObject>
#include <QHostInfo>
#include <QQueue>
#include <QTimer>

class SpotClient : public QObject
{
    Q_OBJECT
public:
    SpotClient(MessageClient *client, QObject *parent = nullptr);

    void prepare();
    void setLocalStation(QString callsign, QString grid, QString info, QString version);
    void enqueueLocalSpot(QString callsign, QString grid, QString info, QString version);
    void enqueueCmd(QString cmd, QString from, QString to, QString relayPath, QString text, QString grid, QString extra, int submode, int dial, int offset, int snr);
    void enqueueSpot(QString callsign, QString grid, int submode, int dial, int offset, int snr);
    void sendRawSpot(QByteArray payload);

public slots:
    void processSpots();
    void dnsLookupResult(QHostInfo const &);

private:
    int m_seq;
    QString m_call;
    QString m_grid;
    QString m_info;
    QString m_version;

    QHostAddress m_address;
    MessageClient *m_client;
    QTimer m_timer;
    QQueue<QByteArray> m_queue;
};

#endif // JS8SPOTCLIENT_H
