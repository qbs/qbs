#ifndef BAR_QT4_H
#define BAR_QT4_H
#include <QObject>

class Bar : public QObject
{
    Q_OBJECT
public:
    explicit Bar(QObject *parent = 0);
};

#endif // BAR_QT4_H
