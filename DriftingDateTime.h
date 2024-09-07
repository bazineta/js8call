#ifndef DRIFTINGDATETIME_H
#define DRIFTINGDATETIME_H

#include <QDateTime>

namespace DriftingDateTime /*: QDateTime*/
{
    QDateTime currentDateTime();
    QDateTime currentDateTimeUtc();
    qint64    currentMSecsSinceEpoch();

    qint64 drift();
    void setDrift(qint64 ms);
    qint64 incrementDrift(qint64 msdelta);
};

#endif // DRIFTINGDATETIME_H
