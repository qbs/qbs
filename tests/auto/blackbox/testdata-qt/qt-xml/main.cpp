#include <QCoreApplication>
#include <QDebug>
#include <QDomDocument>

void testDom()
{
    QDomDocument doc("test");
    QDomElement element = doc.createElement("element");
    element.setAttribute("attr", "foo");
    doc.appendChild(element);
    qDebug() << doc.toString();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("LinkFailureTest");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCoreApplication app(argc, argv);

    testDom();

    return app.exec();
}
