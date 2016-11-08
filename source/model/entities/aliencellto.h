#ifndef ALIENCELLTO_H
#define ALIENCELLTO_H

#include "model/decorators/constants.h"
#include <QVector>
#include <QList>
#include <QVector3D>
#include <QString>

class AlienCell;
struct AlienCellTO  //TO = Transfer Object
{

    AlienCellTO ();
    ~AlienCellTO ();

    void copyCellProperties (const AlienCellTO& otherCell);    //copy internal cell data except computer and token data
    void copyClusterProperties (const AlienCellTO& otherCell); //copy internal cluster data

    //cell properties
    int numCells;
    QVector3D clusterPos;
    QVector3D clusterVel;
    qreal clusterAngle;
    qreal clusterAngVel;
    QVector3D cellPos;
    qreal cellEnergy;
    int cellNumCon;
    int cellMaxCon;
    bool cellAllowToken;
    int cellTokenAccessNum;
    CellFunctionType cellFunctionType;

    //computer data
    QVector< quint8 > computerMemory;
    QString computerCode;

    //token data
    QList< qreal > tokenEnergies;
    QList< QVector< quint8 > > tokenData;
};

#endif // ALIENCELLTO_H

