#ifndef DRIFTINGDATETIME_H
#define DRIFTINGDATETIME_H

#include <QDateTime>

namespace DriftingDateTime /*: QDateTime*/
{
    qint64    drift();
    void      setDrift(qint64);
    qint64    incrementDrift(qint64);
    QDateTime currentDateTime();
    QDateTime currentDateTimeUtc();
    qint64    currentMSecsSinceEpoch();
};

#endif // DRIFTINGDATETIME_H
