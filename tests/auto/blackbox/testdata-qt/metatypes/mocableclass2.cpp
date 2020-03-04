#include <QObject>

class MocableClass2 : public QObject
{
    Q_OBJECT
public:
    MocableClass2(QObject *parent) : QObject(parent) {}
};

#include <mocableclass2.moc>
