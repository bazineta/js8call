#ifndef DRIFTINGDATETIME_H
#define DRIFTINGDATETIME_H

#include <QDateTime>

class DriftingDateTime /*: QDateTime*/
{
public:
    static QDateTime currentDateTime();
    static QDateTime currentDateTimeUtc();
    static qint64 currentMSecsSinceEpoch();

    static qint64 drift();
    static void setDrift(qint64 ms);
    static qint64 incrementDrift(qint64 msdelta);
};

#endif // DRIFTINGDATETIME_H
