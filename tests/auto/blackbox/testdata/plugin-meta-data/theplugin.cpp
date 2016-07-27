#include <QObject>
#include <QtPlugin>

class ThePlugin : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qbs.ThePlugin")
};

#include <theplugin.moc>
