#ifndef TST_BLACKBOX_H
#define TST_BLACKBOX_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QtTest>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

class TestBlackbox : public QObject
{
    Q_OBJECT
    const QString testDataDir;
    const QString testSourceDir;
    const QString buildProfile;
    const QString qbsExecutableFilePath;
    const QString executableSuffix;
    const QString objectSuffix;

public:
    TestBlackbox();

protected:
    int runQbs(QStringList arguments = QStringList(), bool showOutput = false);
    void rmDirR(const QString &dir);
    void touch(const QString &fn);

public slots:
    void initTestCase();
    void init();
    void cleanup();

private slots:
    void build_project_data();
    void build_project();
    void track_qrc();
    void track_qobject_change();
    void trackAddFile();
    void trackRemoveFile();
    void trackAddFileTag();
    void trackRemoveFileTag();
    void trackAddMocInclude();
};

#endif // TST_BLACKBOX_H
