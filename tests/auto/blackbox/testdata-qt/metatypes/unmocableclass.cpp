#include <QObject>

class UnmocableClass : public QObject
{
public:
    UnmocableClass(QObject *parent) : QObject(parent) {}
};
