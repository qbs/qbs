#include <QObject>

class MyObject : public QObject
{
    Q_OBJECT
};

int main()
{
    MyObject obj;
    return 0;
}

#include "bla.moc"

