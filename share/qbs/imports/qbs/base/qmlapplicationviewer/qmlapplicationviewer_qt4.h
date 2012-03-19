#ifndef QMLAPPLICATIONVIEWER_H
#define QMLAPPLICATIONVIEWER_H

#include <QDeclarativeView>

class QmlApplicationViewer : public QDeclarativeView
{
    Q_OBJECT

public:
    enum ScreenOrientation {
        ScreenOrientationLockPortrait,
        ScreenOrientationLockLandscape,
        ScreenOrientationAuto
    };

    explicit QmlApplicationViewer(QWidget *parent = 0);
    virtual ~QmlApplicationViewer();

    static QmlApplicationViewer *create();

    void setMainQmlFile(const QString &file);
    void addImportPath(const QString &path);

    // Note that this will only have an effect on Symbian and Fremantle.
    void setOrientation(ScreenOrientation orientation);

    void showExpanded();

private:
    class QmlApplicationViewerPrivate *d;
};

QApplication *createApplication(int &argc, char **argv);

#endif // QMLAPPLICATIONVIEWER_H
