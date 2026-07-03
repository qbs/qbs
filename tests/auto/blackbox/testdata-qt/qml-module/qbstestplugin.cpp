#include <QQmlExtensionPlugin>

class QbsTestPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *) override {}
};

#include "qbstestplugin.moc"
