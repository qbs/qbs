/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "vsenvironmentdetector.h"

#include "msvcinfo.h"
#include <logging/translator.h>
#include <tools/qbsassert.h>

#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <ShlObj.h>
#endif

namespace qbs {
namespace Internal {

static QString windowsSystem32Path()
{
#ifdef Q_OS_WIN
    wchar_t str[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, str)))
        return QString::fromUtf16(reinterpret_cast<ushort*>(str));
#endif
    return QString();
}

VsEnvironmentDetector::VsEnvironmentDetector(MSVC *msvc)
    : m_msvc(msvc)
    , m_windowsSystemDirPath(windowsSystem32Path())
{
}

bool VsEnvironmentDetector::start()
{
    m_msvc->environments.clear();
    const QString vcvarsallbat = QDir::cleanPath(m_msvc->installPath
                                                     + QLatin1String("/../vcvarsall.bat"));
    if (!QFile::exists(vcvarsallbat)) {
        m_errorString = Tr::tr("Cannot find '%1'.").arg(QDir::toNativeSeparators(vcvarsallbat));
        return false;
    }

    QTemporaryFile tmpFile(QDir::tempPath() + QLatin1Char('/') + QLatin1String("XXXXXX.bat"));
    if (!tmpFile.open()) {
        m_errorString = Tr::tr("Cannot open temporary file '%1' for writing.").arg(
                    tmpFile.fileName());
        return false;
    }

    writeBatchFile(&tmpFile, vcvarsallbat);
    tmpFile.flush();

    QProcess process;
    const QString shellFilePath = QLatin1String("cmd.exe");
    process.start(shellFilePath, QStringList()
                  << QLatin1String("/C") << tmpFile.fileName());
    if (!process.waitForStarted()) {
        m_errorString = Tr::tr("Failed to start '%1'.").arg(shellFilePath);
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitStatus() != QProcess::NormalExit) {
        m_errorString = Tr::tr("Process '%1' did not exit normally.").arg(shellFilePath);
        return false;
    }
    if (process.exitCode() != 0) {
        m_errorString = Tr::tr("Failed to detect Visual Studio environment.");
        return false;
    }
    parseBatOutput(process.readAllStandardOutput());
    return true;
}

static void batClearVars(QTextStream &s, const QStringList &varnames)
{
    foreach (const QString &varname, varnames)
        s << "set " << varname << '=' << endl;
}

static void batPrintVars(QTextStream &s, const QStringList &varnames)
{
    foreach (const QString &varname, varnames)
        s << "echo " << varname << "=%" << varname << '%' << endl;
}

static QString vcArchitecture(MSVC *msvc, const QString &targetArch)
{
    QString vcArch = targetArch;
    if (targetArch == QLatin1String("armv7"))
        vcArch = QLatin1String("arm");
    if (targetArch == QLatin1String("x86_64"))
        vcArch = QLatin1String("amd64");

    // Empty string for the native compiler (preferred)
    for (const QString &hostPrefix :
         QStringList({QString(), QStringLiteral("amd64_"), QStringLiteral("x86_")})) {
        if (QFile::exists(msvc->clPath(hostPrefix + vcArch))) {
            vcArch.prepend(hostPrefix);
            break;
        }
    }

    return vcArch;
}

void VsEnvironmentDetector::writeBatchFile(QIODevice *device, const QString &vcvarsallbat) const
{
    const QStringList varnames = QStringList() << QLatin1String("PATH")
            << QLatin1String("INCLUDE") << QLatin1String("LIB");
    QTextStream s(device);
    s << "@echo off" << endl;
    foreach (const QString &architecture, m_msvc->architectures) {
        s << "echo --" << architecture << "--" << endl
          << "setlocal" << endl;
        batClearVars(s, varnames);
        s << "set PATH=" << m_windowsSystemDirPath << endl; // vcvarsall.bat needs tools from here
        s << "call \"" << vcvarsallbat << "\" " << vcArchitecture(m_msvc, architecture)
          << " || exit /b 1" << endl;
        batPrintVars(s, varnames);
        s << "endlocal" << endl;
    }
}

void VsEnvironmentDetector::parseBatOutput(const QByteArray &output)
{
    QString arch;
    QProcessEnvironment *targetEnv = 0;
    foreach (QByteArray line, output.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith("--") && line.endsWith("--")) {
            line.remove(0, 2);
            line.chop(2);
            arch = QString::fromLocal8Bit(line);
            targetEnv = &m_msvc->environments[arch];
        } else {
            int idx = line.indexOf('=');
            if (idx < 0)
                continue;
            QBS_CHECK(targetEnv);
            const QString name = QString::fromLocal8Bit(line.left(idx));
            QString value = QString::fromLocal8Bit(line.mid(idx + 1));
            if (name.compare(QLatin1String("PATH"), Qt::CaseInsensitive) == 0)
                value.remove(m_windowsSystemDirPath);
            if (value.endsWith(QLatin1Char(';')))
                value.chop(1);
            if (value.endsWith(QLatin1Char('\\')))
                value.chop(1);
            targetEnv->insert(name, value);
        }
    }
}

} // namespace Internal
} // namespace qbs
