#pragma once

#include <QObject>

class Class1 : public QObject
{
    Q_OBJECT

public:
    Class1() = default;

    void foo();
signals:
    void ready();
};
