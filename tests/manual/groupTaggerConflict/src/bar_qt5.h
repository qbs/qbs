#ifndef BAR_QT5_H
#define BAR_QT5_H
#include <QObject>

class Bar : public QObject
{
    Q_OBJECT
public:
    explicit Bar(QObject *parent = 0);
};

#endif // BAR_QT5_H
