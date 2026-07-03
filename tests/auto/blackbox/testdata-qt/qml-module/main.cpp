#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QQmlEngine engine;
    QQmlComponent comp(&engine);
    comp.setData("import io.qt.QbsTest 1.0\nMyItem {}", QUrl("qrc:/"));
    if (comp.isError()) {
        fprintf(stderr, "%s\n", qPrintable(comp.errorString()));
        return 1;
    }
    QScopedPointer<QObject> obj(comp.create());
    if (!obj) {
        fprintf(stderr, "object creation failed: %s\n", qPrintable(comp.errorString()));
        return 1;
    }
    return 0;
}
