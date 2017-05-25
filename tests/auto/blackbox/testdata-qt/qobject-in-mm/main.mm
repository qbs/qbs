#include <QObject>

class Foo : public QObject
{
Q_OBJECT
};

int main()
{
    Foo foo;
    return 0;
}
#include "main.moc"
