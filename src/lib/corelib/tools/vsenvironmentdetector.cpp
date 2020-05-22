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

#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>
#include <tools/stringutils.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qtextstream.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <shlobj.h>
#endif

namespace qbs {
namespace Internal {

static QString windowsSystem32Path()
{
#ifdef Q_OS_WIN
    wchar_t str[UNICODE_STRING_MAX_CHARS];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, str)))
        return QString::fromUtf16(reinterpret_cast<ushort*>(str));
#endif
    return {};
}

VsEnvironmentDetector::VsEnvironmentDetector(QString vcvarsallPath)
    : m_windowsSystemDirPath(windowsSystem32Path())
    , m_vcvarsallPath(std::move(vcvarsallPath))
{
}

bool VsEnvironmentDetector::start(MSVC *msvc)
{
    return start(std::vector<MSVC *>{ msvc });
}

bool VsEnvironmentDetector::start(std::vector<MSVC *> msvcs)
{
    std::sort(msvcs.begin(), msvcs.end(), [] (const MSVC *a, const MSVC *b) -> bool {
        return a->vcInstallPath < b->vcInstallPath;
    });

    std::vector<MSVC *> compatibleMSVCs;
    QString lastVcInstallPath;
    bool someMSVCDetected = false;
    for (MSVC * const msvc : msvcs) {
        if (lastVcInstallPath != msvc->vcInstallPath) {
            lastVcInstallPath = msvc->vcInstallPath;
            if (!compatibleMSVCs.empty()) {
                if (startDetection(compatibleMSVCs))
                    someMSVCDetected = true;
                compatibleMSVCs.clear();
            }
        }
        compatibleMSVCs.push_back(msvc);
    }
    if (startDetection(compatibleMSVCs))
        someMSVCDetected = true;
    return someMSVCDetected;
}

QString VsEnvironmentDetector::findVcVarsAllBat(const MSVC &msvc,
                                                std::vector<QString> &searchedPaths) const
{
    // ### We can only rely on MSVC.vcInstallPath being set
    // when this is called from utilitiesextension.cpp :-(
    // If we knew the vsInstallPath at this point we could just use that
    // instead of searching for vcvarsall.bat candidates.
    QDir dir(msvc.vcInstallPath);
    for (;;) {
        if (!dir.cdUp())
            return {};
        if (dir.dirName() == QLatin1String("VC"))
            break;
    }
    const QString vcvarsallbat = QStringLiteral("vcvarsall.bat");
    QString path = vcvarsallbat;
    QString fullPath = dir.absoluteFilePath(path);
    if (dir.exists(path))
        return fullPath;
    else
        searchedPaths.push_back(fullPath);
    path = QStringLiteral("Auxiliary/Build/") + vcvarsallbat;
    fullPath = dir.absoluteFilePath(path);
    if (dir.exists(path))
        return fullPath;
    else
        searchedPaths.push_back(fullPath);
    return {};
}

bool VsEnvironmentDetector::startDetection(const std::vector<MSVC *> &compatibleMSVCs)
{
    std::vector<QString> searchedPaths;

    if (!m_vcvarsallPath.isEmpty() && !QFileInfo::exists(m_vcvarsallPath)) {
        m_errorString = Tr::tr("%1 does not exist.").arg(m_vcvarsallPath);
        return false;
    }

    const auto vcvarsallbat = !m_vcvarsallPath.isEmpty()
            ? m_vcvarsallPath
            : findVcVarsAllBat(**compatibleMSVCs.begin(), searchedPaths);
    if (vcvarsallbat.isEmpty()) {
        if (!searchedPaths.empty()) {
            m_errorString = Tr::tr(
                        "Cannot find 'vcvarsall.bat' at any of the following locations:\n\t")
                    + join(searchedPaths, QStringLiteral("\n\t"));
        } else {
            m_errorString = Tr::tr("Cannot find 'vcvarsall.bat'.");
        }
        return false;
    }

    QTemporaryFile tmpFile(QDir::tempPath() + QLatin1Char('/') + QStringLiteral("XXXXXX.bat"));
    if (!tmpFile.open()) {
        m_errorString = Tr::tr("Cannot open temporary file '%1' for writing.").arg(
                    tmpFile.fileName());
        return false;
    }

    writeBatchFile(&tmpFile, vcvarsallbat, compatibleMSVCs);
    tmpFile.flush();

    QProcess process;
    static const QString shellFilePath = QStringLiteral("cmd.exe");
    process.start(shellFilePath, QStringList()
                  << QStringLiteral("/C") << tmpFile.fileName());
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
    parseBatOutput(process.readAllStandardOutput(), compatibleMSVCs);
    return true;

}

static void batClearVars(QTextStream &s, const QStringList &varnames)
{
    for (const QString &varname : varnames)
        s << "set " << varname << '=' << Qt::endl;
}

static void batPrintVars(QTextStream &s, const QStringList &varnames)
{
    for (const QString &varname : varnames)
        s << "echo " << varname << "=%" << varname << '%' << Qt::endl;
}

static QString vcArchitecture(const MSVC *msvc)
{
    QString vcArch = msvc->architecture;
    if (msvc->architecture == StringConstants::armv7Arch())
        vcArch = StringConstants::armArch();
    if (msvc->architecture == StringConstants::x86_64Arch())
        vcArch = StringConstants::amd64Arch();

    const QString hostPrefixes[] = {
        StringConstants::x86Arch(),
        QStringLiteral("amd64_"),
        QStringLiteral("x86_")
    };
    for (const QString &hostPrefix : hostPrefixes) {
        if (QFile::exists(msvc->clPathForArchitecture(hostPrefix + vcArch))) {
            vcArch.prepend(hostPrefix);
            break;
        }
    }

    return vcArch;
}

void VsEnvironmentDetector::writeBatchFile(QIODevice *device, const QString &vcvarsallbat,
                                           const std::vector<MSVC *> &msvcs) const
{
    const QStringList varnames = QStringList() << StringConstants::pathEnvVar()
            << QStringLiteral("INCLUDE") << QStringLiteral("LIB") << QStringLiteral("WindowsSdkDir")
            << QStringLiteral("WindowsSDKVersion") << QStringLiteral("VSINSTALLDIR");
    QTextStream s(device);
    using Qt::endl;
    s << "@echo off" << endl;
    // Avoid execution of powershell (in vsdevcmd.bat), which is not in the cleared PATH
    s << "set VSCMD_SKIP_SENDTELEMETRY=1" << endl;
    for (const MSVC *msvc : msvcs) {
        s << "echo --" << msvc->architecture << "--" << endl
          << "setlocal" << endl;
        batClearVars(s, varnames);
        s << "set PATH=" << m_windowsSystemDirPath << endl; // vcvarsall.bat needs tools from here
        s << "call \"" << vcvarsallbat << "\" " << vcArchitecture(msvc)
          << " || exit /b 1" << endl;
        batPrintVars(s, varnames);
        s << "endlocal" << endl;
    }
}

void VsEnvironmentDetector::parseBatOutput(const QByteArray &output, std::vector<MSVC *> msvcs)
{
    QString arch;
    QProcessEnvironment *targetEnv = nullptr;
    const auto lines = output.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith("--") && line.endsWith("--")) {
            line.remove(0, 2);
            line.chop(2);
            arch = QString::fromLocal8Bit(line);
            targetEnv = &msvcs.front()->environment;
            msvcs.erase(msvcs.begin());
        } else {
            int idx = line.indexOf('=');
            if (idx < 0)
                continue;
            QBS_CHECK(targetEnv);
            const QString name = QString::fromLocal8Bit(line.left(idx));
            QString value = QString::fromLocal8Bit(line.mid(idx + 1));
            if (name.compare(StringConstants::pathEnvVar(), Qt::CaseInsensitive) == 0)
                value.remove(m_windowsSystemDirPath);
            if (value.endsWith(QLatin1Char(';')))
                value.chop(1);
            targetEnv->insert(name, value);
        }
    }
}

} // namespace Internal
} // namespace qbs
