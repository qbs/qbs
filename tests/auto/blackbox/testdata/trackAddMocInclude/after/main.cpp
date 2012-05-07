#include <QCoreApplication>

class MyObject : public QObject
{
    Q_OBJECT
public:
    MyObject(QObject *parent = 0)
        : QObject(parent)
    {
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    MyObject *obj = new MyObject(&app);
    return app.exec();
}

#include <main.moc>

