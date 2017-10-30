#include <QObject>

class MyClass1 : public QObject
{
    Q_OBJECT
};

static void f()
{
    MyClass1 m;
}

#include <somefile.moc>
