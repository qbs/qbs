/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

#include "pkgconfigjs.h"

#include <language/scriptengine.h>
#include <tools/version.h>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

#include <QtCore/QProcessEnvironment>

#include <stdexcept>

namespace qbs {
namespace Internal {

namespace {

template<typename C, typename F> QVariantList convert(const C &c, F &&f)
{
    QVariantList result;
    result.reserve(int(c.size()));
    std::transform(c.begin(), c.end(), std::back_inserter(result), f);
    return result;
}

QVariantMap variablesMapToMap(const PcPackage::VariablesMap &variables)
{
    QVariantMap result;
    for (const auto &item: variables)
        result.insert(QString::fromStdString(item.first), QString::fromStdString(item.second));
    return result;
}

QVariantMap packageToMap(const PcPackage &package)
{
    QVariantMap result;
    result[QStringLiteral("isBroken")] = false;
    result[QStringLiteral("filePath")] = QString::fromStdString(package.filePath);
    result[QStringLiteral("baseFileName")] = QString::fromStdString(package.baseFileName);
    result[QStringLiteral("name")] = QString::fromStdString(package.name);
    result[QStringLiteral("version")] = QString::fromStdString(package.version);
    result[QStringLiteral("description")] = QString::fromStdString(package.description);
    result[QStringLiteral("url")] = QString::fromStdString(package.url);

    const auto flagToMap = [](const PcPackage::Flag &flag)
    {
        QVariantMap result;
        const auto value = QString::fromStdString(flag.value);
        result[QStringLiteral("type")] = QVariant::fromValue(qint32(flag.type));
        result[QStringLiteral("value")] = value;
        return result;
    };

    const auto requiredVersionToMap = [](const PcPackage::RequiredVersion &version)
    {
        using Type = PcPackage::RequiredVersion::ComparisonType;
        QVariantMap result;
        result[QStringLiteral("name")] = QString::fromStdString(version.name);
        const auto versionString = QString::fromStdString(version.version);
        const auto qbsVersion = Version::fromString(QString::fromStdString(version.version));
        const auto nextQbsVersion = Version(
                qbsVersion.majorVersion(),
                qbsVersion.minorVersion(),
                qbsVersion.patchLevel() + 1);
        switch (version.comparison) {
        case Type::LessThan:
            result[QStringLiteral("versionBelow")] = versionString;
            break;
        case Type::GreaterThan:
            result[QStringLiteral("versionAtLeast")] = nextQbsVersion.toString();
            break;
        case Type::LessThanEqual:
            result[QStringLiteral("versionBelow")] = nextQbsVersion.toString();
            break;
        case Type::GreaterThanEqual:
            result[QStringLiteral("versionAtLeast")] = versionString;
            break;
        case Type::Equal:
            result[QStringLiteral("version")] = versionString;
            break;
        case Type::NotEqual:
            result[QStringLiteral("versionBelow")] = versionString;
            result[QStringLiteral("versionAtLeast")] = nextQbsVersion.toString();
            break;
        case Type::AlwaysMatch:
            break;
        }
        result[QStringLiteral("comparison")] = QVariant::fromValue(qint32(version.comparison));
        return result;
    };

    result[QStringLiteral("libs")] = convert(package.libs, flagToMap);
    result[QStringLiteral("libsPrivate")] = convert(package.libsPrivate, flagToMap);
    result[QStringLiteral("cflags")] = convert(package.cflags, flagToMap);
    result[QStringLiteral("requires")] = convert(package.requiresPublic, requiredVersionToMap);
    result[QStringLiteral("requiresPrivate")] =
            convert(package.requiresPrivate, requiredVersionToMap);
    result[QStringLiteral("conflicts")] = convert(package.conflicts, requiredVersionToMap);
    result[QStringLiteral("variables")] = variablesMapToMap(package.variables);

    return result;
};

QVariantMap brokenPackageToMap(const PcBrokenPackage &package)
{
    QVariantMap result;
    result[QStringLiteral("isBroken")] = true;
    result[QStringLiteral("filePath")] = QString::fromStdString(package.filePath);
    result[QStringLiteral("baseFileName")] = QString::fromStdString(package.baseFileName);
    result[QStringLiteral("errorText")] = QString::fromStdString(package.errorText);
    return result;
}

QVariantMap packageVariantToMap(const PcPackageVariant &package)
{
    return package.visit([](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, PcPackage>)
            return packageToMap(value);
        else
            return brokenPackageToMap(value);
    });
}

PcPackage::VariablesMap envToVariablesMap(const QProcessEnvironment &env)
{
    PcPackage::VariablesMap result;
    const auto keys = env.keys();
    for (const auto &key : keys)
        result[key.toStdString()] = env.value(key).toStdString();
    return result;
}

PcPackage::VariablesMap variablesFromQVariantMap(const QVariantMap &map)
{
    PcPackage::VariablesMap result;
    for (auto it = map.cbegin(), end = map.cend(); it != end; ++it)
        result[it.key().toStdString()] = it.value().toString().toStdString();
    return result;
}

std::vector<std::string> stringListToStdVector(const QStringList &list)
{
    return transformed<std::vector<std::string>>(list, [](const auto &s) {
        return s.toStdString(); });
}

} // namespace

QScriptValue PkgConfigJs::ctor(QScriptContext *context, QScriptEngine *engine)
{
    try {
        PkgConfigJs *e = nullptr;
        switch (context->argumentCount()) {
        case 0:
            e = new PkgConfigJs(context, engine);
            break;
        case 1:
            e = new PkgConfigJs(context, engine, context->argument(0).toVariant().toMap());
            break;

        default:
            return context->throwError(
                    QStringLiteral("TextFile constructor takes at most three parameters."));
        }

        return engine->newQObject(e, QScriptEngine::ScriptOwnership);
    } catch (const PcException &e) {
        return context->throwError(QString::fromUtf8(e.what()));
    }
}

PkgConfigJs::PkgConfigJs(
        QScriptContext *context, QScriptEngine *engine, const QVariantMap &options) :
    m_pkgConfig(std::make_unique<PkgConfig>(
        convertOptions(static_cast<ScriptEngine *>(engine)->environment(), options)))
{
    Q_UNUSED(context);
    for (const auto &package : m_pkgConfig->packages()) {
        m_packages.insert(
                QString::fromStdString(package.getBaseFileName()), packageVariantToMap(package));
    }
}

PkgConfig::Options PkgConfigJs::convertOptions(const QProcessEnvironment &env, const QVariantMap &map)
{
    PkgConfig::Options result;
    result.libDirs =
            stringListToStdVector(map.value(QStringLiteral("libDirs")).toStringList());
    result.extraPaths =
            stringListToStdVector(map.value(QStringLiteral("extraPaths")).toStringList());
    result.sysroot = map.value(QStringLiteral("sysroot")).toString().toStdString();
    result.topBuildDir = map.value(QStringLiteral("topBuildDir")).toString().toStdString();
    result.allowSystemLibraryPaths =
            map.value(QStringLiteral("allowSystemLibraryPaths"), false).toBool();
    const auto systemLibraryPaths = map.value(QStringLiteral("systemLibraryPaths")).toStringList();
    result.systemLibraryPaths.reserve(systemLibraryPaths.size());
    std::transform(
                systemLibraryPaths.begin(),
                systemLibraryPaths.end(),
                std::back_inserter(result.systemLibraryPaths),
                [](const QString &str){ return str.toStdString(); });
    result.disableUninstalled = map.value(QStringLiteral("disableUninstalled"), true).toBool();
    result.staticMode = map.value(QStringLiteral("staticMode"), false).toBool();
    result.mergeDependencies = map.value(QStringLiteral("mergeDependencies"), true).toBool();
    result.globalVariables =
            variablesFromQVariantMap(map.value(QStringLiteral("globalVariables")).toMap());
    result.systemVariables = envToVariablesMap(env);

    return result;
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionPkgConfig(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(
                &PkgConfigJs::staticMetaObject, engine->newFunction(&PkgConfigJs::ctor));
    extensionObject.setProperty(QStringLiteral("PkgConfig"), obj);
}

Q_DECLARE_METATYPE(qbs::Internal::PkgConfigJs *)
