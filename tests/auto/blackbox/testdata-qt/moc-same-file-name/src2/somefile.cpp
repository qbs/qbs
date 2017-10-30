#include <QObject>

class MyClass2 : public QObject
{
    Q_OBJECT
};

static void f()
{
    MyClass2 m;
}

#include <somefile.moc>
