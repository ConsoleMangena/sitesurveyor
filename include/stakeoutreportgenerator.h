#ifndef STAKEOUTREPORTGENERATOR_H
#define STAKEOUTREPORTGENERATOR_H

#include <QString>
#include <QVector>
#include "stakeoutrecord.h"

class StakeoutReportGenerator
{
public:
    static QString generateHtml(const QVector<StakeoutRecord>& records);
};

#endif // STAKEOUTREPORTGENERATOR_H
