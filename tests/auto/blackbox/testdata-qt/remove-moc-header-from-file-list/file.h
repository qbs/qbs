#pragma once

#include <QObject>

class MyObject : public QObject
{
    Q_OBJECT
  public:
    MyObject();

  signals:
    void someSignal();
};
