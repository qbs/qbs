#include <QObject>

class AnObject : public QObject
{
    Q_OBJECT
signals:
    void someSignal();
};

int main()
{
}

#include <main.moc>
