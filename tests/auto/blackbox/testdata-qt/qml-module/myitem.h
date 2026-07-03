#pragma once
#include <QObject>
#include <QtQml/qqml.h>

class MyItem : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit MyItem(QObject *parent = nullptr);
};
