#include "DriftingDateTime.h"

namespace
{
    qint64 driftms = 0;
}

QDateTime DriftingDateTime::currentDateTime(){
    return QDateTime::currentDateTime().addMSecs(driftms);
}

QDateTime DriftingDateTime::currentDateTimeUtc(){
    return QDateTime::currentDateTimeUtc().addMSecs(driftms);
}

qint64 DriftingDateTime::currentMSecsSinceEpoch(){
    return QDateTime::currentMSecsSinceEpoch() + driftms;
}

qint64 DriftingDateTime::drift(){
    return driftms;
}

void DriftingDateTime::setDrift(qint64 ms){
    driftms = ms;
}

qint64 DriftingDateTime::incrementDrift(qint64 msdelta){
    driftms += msdelta;
    return driftms;
}
