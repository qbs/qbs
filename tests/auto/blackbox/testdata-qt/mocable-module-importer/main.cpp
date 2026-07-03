#include <cstdio>
#include <QObject>

import Mod;

class Receiver : public QObject
{
    Q_OBJECT

public slots:
    void onSignal() { std::puts("received signal"); }
};

int main()
{
    PrimaryObject object;
    Receiver receiver;
}

#include "main.moc"
