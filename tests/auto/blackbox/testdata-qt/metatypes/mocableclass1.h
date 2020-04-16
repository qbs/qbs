#include <QObject>

class MocableClass1 : public QObject
{
    Q_OBJECT
public:
    MocableClass1(QObject *parent = nullptr);
};
