#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPluginLoader>
#include <QtDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QPluginLoader loader(QLatin1String("thePlugin"));
    const QJsonValue v = loader.metaData().value(QLatin1String("theKey"));
    if (!v.isArray()) {
        qDebug() << "value is" << v;
        return 1;
    }
    const QJsonArray a = v.toArray();
    if (a.count() != 1 || a.first() != QLatin1String("theValue")) {
        qDebug() << "value is" << v;
        return 1;
    }
    return 0;
}
