#include <QtGlobal>
#include <QString>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QGuiApplication>
#include <QQmlApplicationEngine>
typedef QGuiApplication Application;
#define AH_SO_THIS_IS_QT5
#else
#include <QApplication>
#include <QDeclarativeView>
#define AH_SO_THIS_IS_QT4
typedef QApplication Application;
#endif

int main(int argc, char *argv[])
{
    Application app(argc, argv);
#ifdef AH_SO_THIS_IS_QT5
    QQmlApplicationEngine engine;
    engine.load(QUrl("blubb"));
#else
    QDeclarativeView view;
    view.setSource(QUrl("blubb"));
#endif

    return app.exec();
}
