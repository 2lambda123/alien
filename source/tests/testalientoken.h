#ifndef TESTALIENTOKEN_H
#define TESTALIENTOKEN_H

#include <QtTest/QtTest>
#include "simulation/entities/alientoken.h"

class TestAlienToken: public QObject
{
    Q_OBJECT
private:
    AlienToken* t;

private slots:
    void initTestCase()
    {
    }

    void testCreation()
    {
        t = new AlienToken(100.0);
        QCOMPARE(t->energy, 100.0);
    }

    void cleanupTestCase()
    {
    }
};

//QTEST_MAIN(TestAlienToken)
//#include "testalientoken.moc"

#endif