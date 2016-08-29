/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <stdio.h>
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTextStream out(stdout);
#ifdef Q_OS_WIN32
    out.setIntegerBase(16);
    out.setNumberFlags(QTextStream::ShowBase);
#ifdef WINVER
    out << "WINVER = " << WINVER;
#else
    out << "WINVER is not defined";
#endif
    endl(out);

    QProcess dumpbin;
    dumpbin.start("dumpbin", QStringList() << "/HEADERS" << a.applicationFilePath());
    dumpbin.waitForStarted();

    out << "dumpbin says...";
    endl(out);

    while (dumpbin.waitForReadyRead()) {
        while (dumpbin.canReadLine()) {
            QString line = dumpbin.readLine();
            if (line.contains("version"))
                out << line.trimmed() << "\n";
        }
    }

    dumpbin.waitForFinished();
#endif

#ifdef Q_OS_MACX
    out.setIntegerBase(10);

    // This gets set by -mmacosx-version-min. If left undefined, defaults to the current OS version.
    out << "__MAC_OS_X_VERSION_MIN_REQUIRED = " << __MAC_OS_X_VERSION_MIN_REQUIRED;
    endl(out);

    // This gets determined by the SDK version you're compiling with
    out << "__MAC_OS_X_VERSION_MAX_ALLOWED = " << __MAC_OS_X_VERSION_MAX_ALLOWED;
    endl(out);

    bool print = false;
    QProcess otool;
    otool.start("otool", QStringList() << "-l" << a.applicationFilePath());
    otool.waitForStarted();

    out << "otool says...";
    endl(out);

    while (otool.waitForReadyRead()) {
        while (otool.canReadLine()) {
            QString line = otool.readLine();
            if (line.contains("LC_VERSION_MIN_MACOSX"))
                print = true;

            if (print && (line.contains("version") || line.contains("sdk"))) {
                out << line.trimmed();
                endl(out);

                if (line.contains("sdk"))
                    print = false;
            }
        }
    }
#endif

    return 0;
}
