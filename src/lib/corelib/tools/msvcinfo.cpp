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

#include "msvcinfo.h"
#include "visualstudioversioninfo.h"

#include <logging/logger.h>
#include <tools/error.h>
#include <tools/profile.h>
#include <tools/stringconstants.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qdir.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtemporaryfile.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

#include <algorithm>
#include <memory>
#include <mutex>

using namespace qbs;
using namespace qbs::Internal;

static std::recursive_mutex envMutex;

static QString mkStr(const char *s) { return QString::fromLocal8Bit(s); }
static QString mkStr(const QByteArray &ba) { return mkStr(ba.constData()); }

class TemporaryEnvChanger
{
public:
    TemporaryEnvChanger(const QProcessEnvironment &envChanges) : m_locker(envMutex)
    {
        QProcessEnvironment currentEnv = QProcessEnvironment::systemEnvironment();
        const auto keys = envChanges.keys();
        for (const QString &key : keys) {
            m_changesToRestore.insert(key, currentEnv.value(key));
            qputenv(qPrintable(key), qPrintable(envChanges.value(key)));
        }
    }

    ~TemporaryEnvChanger()
    {
        const auto keys = m_changesToRestore.keys();
        for (const QString &key : keys)
            qputenv(qPrintable(key), qPrintable(m_changesToRestore.value(key)));
    }

private:
    QProcessEnvironment m_changesToRestore;
    std::lock_guard<std::recursive_mutex> m_locker;
};

static QByteArray runProcess(const QString &exeFilePath, const QStringList &args,
                             const QProcessEnvironment &env = QProcessEnvironment(),
                             bool allowFailure = false,
                             const QByteArray &pipeData = QByteArray())
{
    TemporaryEnvChanger envChanger(env);
    QProcess process;
    process.start(exeFilePath, args);
    if (!process.waitForStarted())
        throw ErrorInfo(mkStr("Could not start %1 (%2)").arg(exeFilePath, process.errorString()));
    if (!pipeData.isEmpty()) {
        process.write(pipeData);
        process.closeWriteChannel();
    }
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit)
        throw ErrorInfo(mkStr("Could not run %1 (%2)").arg(exeFilePath, process.errorString()));
    if (process.exitCode() != 0 && !allowFailure) {
        ErrorInfo e(mkStr("Process '%1' failed with exit code %2.")
                    .arg(exeFilePath).arg(process.exitCode()));
        const QByteArray stdErr = process.readAllStandardError();
        if (!stdErr.isEmpty())
            e.append(mkStr("stderr was: %1").arg(mkStr(stdErr)));
        const QByteArray stdOut = process.readAllStandardOutput();
        if (!stdOut.isEmpty())
            e.append(mkStr("stdout was: %1").arg(mkStr(stdOut)));
        throw e;
    }
    return process.readAllStandardOutput().trimmed();
}

class DummyFile {
public:
    DummyFile(QString fp) : filePath(std::move(fp)) { }
    ~DummyFile() { QFile::remove(filePath); }
    const QString filePath;
};

#ifdef Q_OS_WIN
static QStringList parseCommandLine(const QString &commandLine)
{
    const auto buf = std::make_unique<wchar_t[]>(size_t(commandLine.size()) + 1);
    buf[size_t(commandLine.toWCharArray(buf.get()))] = 0;
    int argCount = 0;
    const auto argsDeleter = [](LPWSTR *p){ LocalFree(p); };
    const auto args = std::unique_ptr<LPWSTR[], decltype(argsDeleter)>(
            CommandLineToArgvW(buf.get(), &argCount), argsDeleter);
    if (!args)
        throw ErrorInfo(mkStr("Could not parse command line arguments: ") + commandLine);
    QStringList list;
    list.reserve(argCount);
    for (int i = 0; i < argCount; ++i)
        list.push_back(QString::fromWCharArray(args[size_t(i)]));
    return list;
}
#endif

static QVariantMap getMsvcDefines(const QString &compilerFilePath,
                                  const QProcessEnvironment &compilerEnv,
                                  MSVC::CompilerLanguage language)
{
#ifdef Q_OS_WIN
    QString backendSwitch, languageSwitch;
    switch (language) {
    case MSVC::CLanguage:
        backendSwitch = QStringLiteral("/B1");
        languageSwitch = QStringLiteral("/TC");
        break;
    case MSVC::CPlusPlusLanguage:
        backendSwitch = QStringLiteral("/Bx");
        languageSwitch = QStringLiteral("/TP");
        break;
    }
    const QByteArray commands("set MSC_CMD_FLAGS\n");
    QStringList out = QString::fromLocal8Bit(runProcess(compilerFilePath, QStringList()
               << QStringLiteral("/nologo")
               << backendSwitch
               << qEnvironmentVariable("COMSPEC")
               << QStringLiteral("/c")
               << languageSwitch
               << QStringLiteral("NUL"),
               compilerEnv, true, commands)).split(QLatin1Char('\n'));

    auto findResult = std::find_if(out.cbegin(), out.cend(), [] (const QString &line) {
            return line.startsWith(QLatin1String("MSC_CMD_FLAGS="));
        });
    if (findResult == out.cend()) {
        throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                        + out.join(QLatin1Char('\n')));
    }

    QVariantMap map;
    const QStringList args = parseCommandLine(findResult->trimmed());
    for (const QString &arg : args) {
        if (!arg.startsWith(QStringLiteral("-D")))
            continue;
        int idx = arg.indexOf(QLatin1Char('='), 2);
        if (idx > 2)
            map.insert(arg.mid(2, idx - 2), arg.mid(idx + 1));
        else
            map.insert(arg.mid(2), QVariant());
    }

    return map;
#else
    Q_UNUSED(compilerFilePath);
    Q_UNUSED(compilerEnv);
    Q_UNUSED(language);
    return {};
#endif
}

/*!
    \internal
    clang-cl does not support gcc and msvc ways to dump a macros, so we have to use original
    clang.exe directly
*/
static QVariantMap getClangClDefines(
        const QString &compilerFilePath,
        const QProcessEnvironment &compilerEnv,
        MSVC::CompilerLanguage language,
        const QString &arch)
{
#ifdef Q_OS_WIN
    QFileInfo clInfo(compilerFilePath);
    QFileInfo clangInfo(clInfo.absolutePath() + QLatin1String("/clang-cl.exe"));
    if (!clangInfo.exists())
        throw ErrorInfo(QStringLiteral("%1 does not exist").arg(clangInfo.absoluteFilePath()));

    QStringList args = {
        QStringLiteral("/d1PP"), // dump macros
        QStringLiteral("/E") // preprocess to stdout
    };

    if (language == MSVC::CLanguage)
        args.append(QStringLiteral("/TC"));
    else if (language == MSVC::CPlusPlusLanguage)
        args.append(QStringLiteral("/TP"));

    if (arch == QLatin1String("x86"))
        args.append(QStringLiteral("-m32"));
    else if (arch == QLatin1String("x86_64"))
        args.append(QStringLiteral("-m64"));

    args.append(QStringLiteral("NUL")); // filename

    const auto lines = QString::fromLocal8Bit(
            runProcess(
                    clangInfo.absoluteFilePath(),
                    args,
                    compilerEnv,
                    true)).split(QLatin1Char('\n'));
    QVariantMap result;
    for (const auto &line: lines) {
        static const auto defineString = QLatin1String("#define ");
        if (!line.startsWith(defineString))
            continue;
        QStringView view(line.data() + defineString.size());
        const auto it = std::find(view.begin(), view.end(), QLatin1Char(' '));
        if (it == view.end()) {
            throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                            + lines.join(QLatin1Char('\n')));
        }
        QStringView key(view.begin(), it);
        QStringView value(it + 1, view.end());
        result.insert(key.toString(), value.isEmpty() ? QVariant() : QVariant(value.toString()));
    }
    if (result.isEmpty()) {
        throw ErrorInfo(QStringLiteral("Cannot determine macroses from compiler frontend output: ")
                        + lines.join(QLatin1Char('\n')));
    }
    return result;
#else
    Q_UNUSED(compilerFilePath);
    Q_UNUSED(compilerEnv);
    Q_UNUSED(language);
    Q_UNUSED(arch);
    return {};
#endif
}

static QString formatVswhereOutput(const QString &out, const QString &err)
{
    QString ret;
    if (!out.isEmpty()) {
        ret.append(Tr::tr("stdout")).append(QLatin1String(":\n"));
        const auto lines = out.split(QLatin1Char('\n'));
        for (const QString &line : lines)
            ret.append(QLatin1Char('\t')).append(line).append(QLatin1Char('\n'));
    }
    if (!err.isEmpty()) {
        ret.append(Tr::tr("stderr")).append(QLatin1String(":\n"));
        const auto lines = err.split(QLatin1Char('\n'));
        for (const QString &line : lines)
            ret.append(QLatin1Char('\t')).append(line).append(QLatin1Char('\n'));
    }
    return ret;
}

static QString wow6432Key()
{
#ifdef Q_OS_WIN64
    return QStringLiteral("\\Wow6432Node");
#else
    return {};
#endif
}

static QString vswhereFilePath()
{
    static const std::vector<const char *> envVarCandidates{"ProgramFiles", "ProgramFiles(x86)"};
    for (const char * const envVar : envVarCandidates) {
        const QString value = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv(envVar)));
        const QString cmd = value
                + QStringLiteral("/Microsoft Visual Studio/Installer/vswhere.exe");
        if (QFileInfo(cmd).exists())
            return cmd;
    }
    return {};
}

enum class ProductType { VisualStudio, BuildTools };
static std::vector<MSVCInstallInfo> retrieveInstancesFromVSWhere(
        ProductType productType, Logger &logger)
{
    std::vector<MSVCInstallInfo> result;
    const QString cmd = vswhereFilePath();
    if (cmd.isEmpty())
        return result;
    QProcess vsWhere;
    QStringList args = productType == ProductType::VisualStudio
            ? QStringList({QStringLiteral("-all"), QStringLiteral("-legacy"),
                           QStringLiteral("-prerelease")})
            : QStringList({QStringLiteral("-products"),
                           QStringLiteral("Microsoft.VisualStudio.Product.BuildTools")});
    args << QStringLiteral("-format") << QStringLiteral("json") << QStringLiteral("-utf8");
    vsWhere.start(cmd, args);
    if (!vsWhere.waitForStarted(-1))
        return result;
    if (!vsWhere.waitForFinished(-1)) {
        logger.qbsWarning() << Tr::tr("The vswhere tool failed to run").append(QLatin1String(": "))
                        .append(vsWhere.errorString());
        return result;
    }
    if (vsWhere.exitCode() != 0) {
        const QString stdOut = QString::fromLocal8Bit(vsWhere.readAllStandardOutput());
        const QString stdErr = QString::fromLocal8Bit(vsWhere.readAllStandardError());
        logger.qbsWarning() << Tr::tr("The vswhere tool failed to run").append(QLatin1String(".\n"))
                     .append(formatVswhereOutput(stdOut, stdErr));
        return result;
    }
    QJsonParseError parseError{};
    QJsonDocument jsonOutput = QJsonDocument::fromJson(vsWhere.readAllStandardOutput(),
                                                       &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        logger.qbsWarning() << Tr::tr("The vswhere tool produced invalid JSON output: %1")
                        .arg(parseError.errorString());
        return result;
    }
    const auto jsonArray = jsonOutput.array();
    for (const QJsonValue &v : jsonArray) {
        const QJsonObject o = v.toObject();
        MSVCInstallInfo info;
        info.version = o.value(QStringLiteral("installationVersion")).toString();
        if (productType == ProductType::BuildTools) {
            // For build tools, the version is e.g. "15.8.28010.2036", rather than "15.0".
            const int dotIndex = info.version.indexOf(QLatin1Char('.'));
            if (dotIndex != -1)
                info.version = info.version.left(dotIndex);
        }
        info.installDir = o.value(QStringLiteral("installationPath")).toString();
        if (!info.version.isEmpty() && !info.installDir.isEmpty())
            result.push_back(info);
    }
    return result;
}

static std::vector<MSVCInstallInfo> installedMSVCsFromVsWhere(Logger &logger)
{
    const std::vector<MSVCInstallInfo> vsInstallations
            = retrieveInstancesFromVSWhere(ProductType::VisualStudio, logger);
    const std::vector<MSVCInstallInfo> buildToolInstallations
            = retrieveInstancesFromVSWhere(ProductType::BuildTools, logger);
    std::vector<MSVCInstallInfo> all;
    std::copy(vsInstallations.begin(), vsInstallations.end(), std::back_inserter(all));
    std::copy(buildToolInstallations.begin(), buildToolInstallations.end(),
              std::back_inserter(all));
    return all;
}

static std::vector<MSVCInstallInfo> installedMSVCsFromRegistry()
{
    std::vector<MSVCInstallInfo> result;

    // Detect Visual Studio
    const QSettings vsRegistry(
                QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                + QStringLiteral("\\Microsoft\\VisualStudio\\SxS\\VS7"),
                QSettings::NativeFormat);
    const auto vsNames = vsRegistry.childKeys();
    for (const QString &vsName : vsNames) {
        MSVCInstallInfo entry;
        entry.version = vsName;
        entry.installDir = vsRegistry.value(vsName).toString();
        result.push_back(entry);
    }

    // Detect Visual C++ Build Tools
    QSettings vcbtRegistry(
                QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                + QStringLiteral("\\Microsoft\\VisualCppBuildTools"),
                QSettings::NativeFormat);
    const QStringList &vcbtRegistryChildGroups = vcbtRegistry.childGroups();
    for (const QString &childGroup : vcbtRegistryChildGroups) {
        vcbtRegistry.beginGroup(childGroup);
        bool ok;
        int installed = vcbtRegistry.value(QStringLiteral("Installed")).toInt(&ok);
        if (ok && installed) {
            MSVCInstallInfo entry;
            entry.version = childGroup;
            const QSettings vsRegistry(
                        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                        + QStringLiteral("\\Microsoft\\VisualStudio\\") + childGroup
                        + QStringLiteral("\\Setup\\VC"),
                        QSettings::NativeFormat);
            entry.installDir = vsRegistry.value(QStringLiteral("ProductDir")).toString();
            result.push_back(entry);
        }
        vcbtRegistry.endGroup();
    }

    return result;
}

/*
    Returns the list of compilers present in all MSVC installations
    (Visual Studios or Build Tools) without the architecture, e.g.
    [VC\Tools\MSVC\14.16.27023, VC\Tools\MSVC\14.14.26428, ...]
*/
static std::vector<MSVC> installedCompilersHelper(Logger &logger)
{
    std::vector<MSVC> msvcs;
    std::vector<MSVCInstallInfo> installInfos = installedMSVCsFromVsWhere(logger);
    if (installInfos.empty())
        installInfos = installedMSVCsFromRegistry();
    for (const MSVCInstallInfo &installInfo : installInfos) {
        MSVC msvc;
        msvc.internalVsVersion = Version::fromString(installInfo.version, true);
        if (!msvc.internalVsVersion.isValid())
            continue;

        QDir vsInstallDir(installInfo.installDir);
        msvc.vsInstallPath = vsInstallDir.absolutePath();
        if (vsInstallDir.dirName() != QStringLiteral("VC")
                && !vsInstallDir.cd(QStringLiteral("VC"))) {
            continue;
        }

        msvc.version = QString::number(Internal::VisualStudioVersionInfo(
            msvc.internalVsVersion).marketingVersion());
        if (msvc.version.isEmpty()) {
            logger.qbsWarning()
                    << Tr::tr("Unknown MSVC version %1 found.").arg(installInfo.version);
            continue;
        }

        if (msvc.internalVsVersion.majorVersion() < 15) {
            QDir vcInstallDir = vsInstallDir;
            if (!vcInstallDir.cd(QStringLiteral("bin")))
                continue;
            msvc.vcInstallPath = vcInstallDir.absolutePath();
            msvcs.push_back(msvc);
        } else {
            QDir vcInstallDir = vsInstallDir;
            vcInstallDir.cd(QStringLiteral("Tools/MSVC"));
            const auto vcVersionStrs = vcInstallDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &vcVersionStr : vcVersionStrs) {
                const Version vcVersion = Version::fromString(vcVersionStr);
                if (!vcVersion.isValid())
                    continue;
                QDir specificVcInstallDir = vcInstallDir;
                if (!specificVcInstallDir.cd(vcVersionStr)
                    || !specificVcInstallDir.cd(QStringLiteral("bin"))) {
                    continue;
                }
                msvc.vcInstallPath = specificVcInstallDir.absolutePath();
                msvcs.push_back(msvc);
            }
        }
    }
    return msvcs;
}

void MSVC::init()
{
    determineCompilerVersion();
}

/*!
  \internal
  Returns the architecture detected from the compiler path.
*/
QString MSVC::architectureFromClPath(const QString &clPath)
{
    const auto parentDir = QFileInfo(clPath).absolutePath();
    const auto parentDirName = QFileInfo(parentDir).fileName().toLower();
    if (parentDirName == QLatin1String("bin"))
        return QStringLiteral("x86");
    return parentDirName;
}

QString MSVC::canonicalArchitecture(const QString &arch)
{
    if (arch == QLatin1String("x64") || arch == QLatin1String("amd64"))
        return QStringLiteral("x86_64");
    return arch;
}

std::pair<QString, QString> MSVC::getHostTargetArchPair(const QString &arch)
{
    QString hostArch;
    QString targetArch;
    const int index = arch.indexOf(QLatin1Char('_'));
    if (index != -1) {
        hostArch = arch.mid(0, index);
        targetArch = arch.mid(index);
    } else {
        hostArch = arch;
        targetArch = arch;
    }
    return {canonicalArchitecture(hostArch), canonicalArchitecture(targetArch)};
}

QString MSVC::binPathForArchitecture(const QString &arch) const
{
    QString archSubDir;
    if (arch != StringConstants::x86Arch())
        archSubDir = arch;
    return QDir::cleanPath(vcInstallPath + QLatin1Char('/') + pathPrefix + QLatin1Char('/')
                           + archSubDir);
}

static QString clExeSuffix() { return QStringLiteral("/cl.exe"); }

QString MSVC::clPathForArchitecture(const QString &arch) const
{
    return binPathForArchitecture(arch) + clExeSuffix();
}

QVariantMap MSVC::compilerDefines(const QString &compilerFilePath,
                                  MSVC::CompilerLanguage language) const
{
    const auto compilerName = QFileInfo(compilerFilePath).fileName().toLower();
    if (compilerName == QLatin1String("clang-cl.exe"))
        return getClangClDefines(compilerFilePath, environment, language, architecture);
    return getMsvcDefines(compilerFilePath, environment, language);
}

std::vector<MSVCArchInfo> MSVC::findSupportedArchitectures(const MSVC &msvc)
{
    std::vector<MSVCArchInfo> result;
    auto addResult = [&result](const MSVCArchInfo &ai) {
        if (QFile::exists(ai.binPath + QLatin1String("/cl.exe")))
            result.push_back(ai);
    };
    if (msvc.internalVsVersion.majorVersion() < 15) {
        static const QStringList knownArchitectures = QStringList()
            << QStringLiteral("x86")
            << QStringLiteral("amd64_x86")
            << QStringLiteral("amd64")
            << QStringLiteral("x86_amd64")
            << QStringLiteral("ia64")
            << QStringLiteral("x86_ia64")
            << QStringLiteral("x86_arm")
            << QStringLiteral("amd64_arm");
        for (const QString &knownArchitecture : knownArchitectures) {
            MSVCArchInfo ai;
            ai.arch = knownArchitecture;
            ai.binPath = msvc.binPathForArchitecture(knownArchitecture);
            addResult(ai);
        }
    } else {
        QDir vcInstallDir(msvc.vcInstallPath);
        const auto hostArchs = vcInstallDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &hostArch : hostArchs) {
            QDir subdir = vcInstallDir;
            if (!subdir.cd(hostArch))
                continue;
            const QString shortHostArch = hostArch.mid(4).toLower();
            const auto archs = subdir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &arch : archs) {
                MSVCArchInfo ai;
                ai.binPath = subdir.absoluteFilePath(arch);
                if (shortHostArch == arch)
                    ai.arch = arch;
                else
                    ai.arch = shortHostArch + QLatin1Char('_') + arch;
                addResult(ai);
            }
        }
    }
    return result;
}

QVariantMap MSVC::toVariantMap() const
{
    return {
        {QStringLiteral("version"), version},
        {QStringLiteral("internalVsVersion"), internalVsVersion.toString()},
        {QStringLiteral("vsInstallPath"), vsInstallPath},
        {QStringLiteral("vcInstallPath"), vcInstallPath},
        {QStringLiteral("binPath"), binPath},
        {QStringLiteral("architecture"), architecture},
    };
}

/*!
    \internal
    Returns the list of all compilers present in all MSVC installations
    separated by host/target arch, e.g.
    [
        VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64,
        VC\Tools\MSVC\14.16.27023\bin\Hostx64\x86,
        VC\Tools\MSVC\14.16.27023\bin\Hostx64\arm,
        VC\Tools\MSVC\14.16.27023\bin\Hostx64\arm64,
        VC\Tools\MSVC\14.16.27023\bin\Hostx86\x64,
        ...
    ]
    \note that MSVC.architecture can be either "x64" or "amd64" (depending on the MSVC version)
    in case of 64-bit platform (but we use the "x86_64" name...)
*/
std::vector<MSVC> MSVC::installedCompilers(Logger &logger)
{
    std::vector<MSVC> msvcs;
    const auto instMsvcs = installedCompilersHelper(logger);
    for (const MSVC &msvc : instMsvcs) {
        if (msvc.internalVsVersion.majorVersion() < 15) {
            // Check existence of various install scripts
            const QString vcvars32bat = msvc.vcInstallPath + QLatin1String("/vcvars32.bat");
            if (!QFileInfo(vcvars32bat).isFile())
                continue;
        }

        const auto ais = findSupportedArchitectures(msvc);
        for (const MSVCArchInfo &ai : ais) {
            MSVC specificMSVC = msvc;
            specificMSVC.architecture = ai.arch;
            specificMSVC.binPath = ai.binPath;
            msvcs.push_back(specificMSVC);
        }
    }
    return msvcs;
}

void MSVC::determineCompilerVersion()
{
    QString cppFilePath;
    {
        QTemporaryFile cppFile(QDir::tempPath() + QLatin1String("/qbsXXXXXX.cpp"));
        cppFile.setAutoRemove(false);
        if (!cppFile.open()) {
            throw ErrorInfo(mkStr("Could not create temporary file (%1)")
                            .arg(cppFile.errorString()));
        }
        cppFilePath = cppFile.fileName();
        cppFile.write("_MSC_FULL_VER");
        cppFile.close();
    }
    DummyFile fileDeleter(cppFilePath);

    std::lock_guard<std::recursive_mutex> locker(envMutex);
    const QByteArray origPath = qgetenv("PATH");
    qputenv("PATH", environment.value(StringConstants::pathEnvVar()).toLatin1() + ';' + origPath);
    QByteArray versionStr = runProcess(
                binPath + clExeSuffix(),
                QStringList() << QStringLiteral("/nologo") << QStringLiteral("/EP")
                << QDir::toNativeSeparators(cppFilePath));
    qputenv("PATH", origPath);
    compilerVersion = Version(versionStr.mid(0, 2).toInt(), versionStr.mid(2, 2).toInt(),
                              versionStr.mid(4).toInt());
}

QString MSVCInstallInfo::findVcvarsallBat() const
{
    static const auto vcvarsall2017 = QStringLiteral("VC/Auxiliary/Build/vcvarsall.bat");
    // 2015, 2013 and 2012
    static const auto vcvarsallOld = QStringLiteral("VC/vcvarsall.bat");
    QDir dir(installDir);
    if (dir.exists(vcvarsall2017))
        return dir.absoluteFilePath(vcvarsall2017);
    if (dir.exists(vcvarsallOld))
        return dir.absoluteFilePath(vcvarsallOld);
    return {};
}

std::vector<MSVCInstallInfo> MSVCInstallInfo::installedMSVCs(Logger &logger)
{
    const auto installInfos = installedMSVCsFromVsWhere(logger);
    if (installInfos.empty())
        return installedMSVCsFromRegistry();
    return installInfos;
}
