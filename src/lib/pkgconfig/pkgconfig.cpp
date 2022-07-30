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

#include "pkgconfig.h"
#include "pcparser.h"

#if HAS_STD_FILESYSTEM
#  if __has_include(<filesystem>)
#    include <filesystem>
#  else
#    include <experimental/filesystem>
// We need the alias from std::experimental::filesystem to std::filesystem
namespace std {
    namespace filesystem = experimental::filesystem;
}
#  endif
#else
#  include <QtCore/QDir>
#  include <QtCore/QFileInfo>
#endif

#include <algorithm>
#include <iostream>

namespace qbs {

namespace {

std::string varToEnvVar(std::string_view pkg, std::string_view var)
{
    auto result = std::string("PKG_CONFIG_");
    result += pkg;
    result += '_';
    result += var;

    for (char &p : result) {
        int c = std::toupper(p);

        if (!std::isalnum(c))
            c = '_';

        p = char(c);
    }

    return result;
}

std::vector<std::string> split(std::string_view str, const char delim)
{
    std::vector<std::string> result;
    size_t prev = 0;
    size_t pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token(str.substr(prev, pos - prev));
        if (!token.empty())
            result.push_back(token);
        prev = pos + 1;
    } while (pos < str.length() && prev < str.length());
    return result;
}

constexpr inline char listSeparator() noexcept
{
#if defined(WIN32)
    return ';';
#else
    return ':';
#endif
}

// based on https://stackoverflow.com/a/33135699/295518
int compareVersions(std::string_view v1, std::string_view v2)
{
    for (size_t i = 0, j = 0; i < v1.length() || j < v2.length(); ) {
        size_t acc1 = 0;
        size_t acc2 = 0;

        while (i < v1.length() && v1[i] != '.') {
            acc1 = acc1 * 10 + (v1[i] - '0');
            i++;
        }
        while (j < v2.length() && v2[j] != '.') {
            acc2 = acc2 * 10 + (v2[j] - '0');
            j++;
        }

        if (acc1 < acc2)
            return -1;
        if (acc1 > acc2)
            return +1;

        ++i;
        ++j;
    }
    return 0;
}

using ComparisonType = PcPackage::RequiredVersion::ComparisonType;

bool versionTest(ComparisonType comparison, std::string_view a, std::string_view b)
{
    switch (comparison) {
    case ComparisonType::LessThan: return compareVersions(a, b) < 0;
    case ComparisonType::GreaterThan: return compareVersions(a, b) > 0;
    case ComparisonType::LessThanEqual: return compareVersions(a, b) <= 0;
    case ComparisonType::GreaterThanEqual: return compareVersions(a, b) >= 0;
    case ComparisonType::Equal: return compareVersions(a, b) == 0;
    case ComparisonType::NotEqual: return compareVersions(a, b) != 0;
    case ComparisonType::AlwaysMatch: return true;
    }

    return false;
}

[[noreturn]] void raizeUnknownPackageException(std::string_view package)
{
    std::string message;
    message += "Can't find package '";
    message += package;
    message += "'";
    throw PcException(message);
}

template <class C>
C &operator<<(C &container, const C &other)
{
    container.insert(container.end(), other.cbegin(), other.cend());
    return container;
}

} // namespace

PkgConfig::PkgConfig()
    : PkgConfig(Options())
{
}

PkgConfig::PkgConfig(Options options)
    : m_options(std::move(options))
{
    if (m_options.libDirs.empty())
        m_options.libDirs = split(PKG_CONFIG_PC_PATH, listSeparator());

    if (m_options.topBuildDir.empty())
        m_options.topBuildDir = "$(top_builddir)"; // pkg-config sets this for automake =)

    if (m_options.systemLibraryPaths.empty())
        m_options.systemLibraryPaths = split(PKG_CONFIG_SYSTEM_LIBRARY_PATH, ':');

    // this is weird on Windows, but that's what pkg-config does
    if (m_options.sysroot.empty())
        m_options.globalVariables["pc_sysrootdir"] = "/";
    else
        m_options.globalVariables["pc_sysrootdir"] = m_options.sysroot;
    m_options.globalVariables["pc_top_builddir"] = m_options.topBuildDir;

    m_packages = findPackages();
}

const PcPackageVariant &PkgConfig::getPackage(std::string_view baseFileName) const
{
    // heterogeneous comparator so we can search the package using string_view
    const auto lessThan = [](const PcPackageVariant &package, const std::string_view &name)
    {
        return package.visit([name](auto &&value) noexcept {
            return value.baseFileName < name;
        });
    };

    const auto testPackage = [baseFileName](const PcPackageVariant &package) {
        return package.visit([baseFileName](auto &&value) noexcept {
            return baseFileName != value.baseFileName;
        });
    };

    const auto it = std::lower_bound(m_packages.begin(), m_packages.end(), baseFileName, lessThan);
    if (it == m_packages.end() || testPackage(*it))
        raizeUnknownPackageException(baseFileName);
    return *it;
}

std::optional<std::string_view> PkgConfig::packageGetVariable(
    const PcPackage &pkg, std::string_view var) const
{
    std::optional<std::string_view> result;

    if (var.empty())
        return result;

    const auto &globals = m_options.globalVariables;
    if (auto it = globals.find(var); it != globals.end())
        result = it->second;

    // Allow overriding specific variables using an environment variable of the
    // form PKG_CONFIG_$PACKAGENAME_$VARIABLE
    if (!pkg.baseFileName.empty()) {
        const std::string envVariable = varToEnvVar(pkg.baseFileName, var);
        const auto it = m_options.systemVariables.find(envVariable);
        if (it != m_options.systemVariables.end())
            result = it->second;
    }

    if (!result) {
        if (const auto it = pkg.variables.find(var); it != pkg.variables.end())
            result = it->second;
    }

    return result;
}

#if HAS_STD_FILESYSTEM
std::vector<std::string> getPcFilePaths(const std::vector<std::string> &searchPaths)
{
    std::vector<std::filesystem::path> paths;

    for (const auto &searchPath : searchPaths) {
        if (!std::filesystem::exists(std::filesystem::directory_entry(searchPath).status()))
            continue;
        const auto dir = std::filesystem::directory_iterator(searchPath);
        std::copy_if(
            std::filesystem::begin(dir),
            std::filesystem::end(dir),
            std::back_inserter(paths),
            [](const auto &entry) { return entry.path().extension() == ".pc"; }
        );
    }
    std::vector<std::string> result;
    std::transform(
        std::begin(paths),
        std::end(paths),
        std::back_inserter(result),
        [](const auto &path) { return path.generic_string(); }
    );
    return result;
}
#else
std::vector<std::string> getPcFilePaths(const std::vector<std::string> &searchPaths)
{
    std::vector<std::string> result;
    for (const auto &path : searchPaths) {
        QDir dir(QString::fromStdString(path));
        const auto paths = dir.entryList({QStringLiteral("*.pc")});
        std::transform(
            std::begin(paths),
            std::end(paths),
            std::back_inserter(result),
            [&dir](const auto &path) { return dir.filePath(path).toStdString(); }
        );
    }
    return result;
}
#endif

PcBrokenPackage makeMissingDependency(
    const PcPackage &package, const PcPackage::RequiredVersion &depVersion)
{
    std::string message;
    message += "Package ";
    message += package.name;
    message += " requires package ";
    message += depVersion.name;
    message += " but it is not found";
    return PcBrokenPackage{
            package.filePath, package.baseFileName, std::move(message)};
}

PcBrokenPackage makeBrokenDependency(
    const PcPackage &package, const PcPackage::RequiredVersion &depVersion)
{
    std::string message;
    message += "Package ";
    message += package.name;
    message += " requires package ";
    message += depVersion.name;
    message += " but it is broken";
    return PcBrokenPackage{
            package.filePath, package.baseFileName, std::move(message)};
}

PcBrokenPackage makeVersionMismatchDependency(
    const PcPackage &package,
    const PcPackage &depPackage,
    const PcPackage::RequiredVersion &depVersion)
{
    std::string message;
    message += "Package ";
    message += package.name;
    message += " requires version ";
    message += PcPackage::RequiredVersion::comparisonToString(
            depVersion.comparison);
    message += depVersion.version;
    message += " but ";
    message += depPackage.version;
    message += " is present";
    return PcBrokenPackage{
            package.filePath, package.baseFileName, std::move(message)};
}

PkgConfig::Packages PkgConfig::mergeDependencies(const PkgConfig::Packages &packages) const
{
    std::unordered_map<std::string_view, const PcPackageVariant *> packageHash;

    struct MergedHashEntry
    {
        PcPackageVariant package; // merged package or broken package
        std::vector<const PcPackage *> deps; // unmerged transitive deps, including Package itself
    };
    std::unordered_map<std::string, MergedHashEntry> mergedHash;

    for (const auto &package: packages)
        packageHash[package.getBaseFileName()] = &package;

    auto func = [&](const PcPackageVariant &package, auto &f) -> const MergedHashEntry &
    {
        const auto it = mergedHash.find(package.getBaseFileName());
        if (it != mergedHash.end())
            return it->second;

        auto &entry = mergedHash[package.getBaseFileName()];

        auto visitor = [&](auto &&package) -> PcPackageVariant {

            using T = std::decay_t<decltype(package)>;
            if constexpr (std::is_same_v<T, PcPackage>) { // NOLINT

                using Flags = std::vector<PcPackage::Flag>;

                // returns true if multiple copies of the flag can present in the same package
                // we can't properly merge flags that have multiple parameters except for
                // -framework which we handle correctly.
                auto canHaveDuplicates = [](const PcPackage::Flag::Type &type) {
                    return type == PcPackage::Flag::Type::LinkerFlag
                            || type == PcPackage::Flag::Type::CompilerFlag;
                };

                std::unordered_set<PcPackage::Flag> visitedFlags;
                // appends only those flags to the target that were not seen before (except for
                // ones that can have duplicates)
                auto mergeFlags = [&](Flags &target, const Flags &source)
                {
                    for (const auto &flag: source) {
                        if (canHaveDuplicates(flag.type) || visitedFlags.insert(flag).second)
                            target.push_back(flag);
                    }
                };

                std::unordered_set<const PcPackage *> visitedDeps;

                PcPackage result;
                // copy only meta info for now
                result.filePath = package.filePath;
                result.baseFileName = package.baseFileName;
                result.name = package.name;
                result.version = package.version;
                result.description = package.description;
                result.url = package.url;
                result.variables = package.variables;

                auto allDependencies = package.requiresPublic;
                if (m_options.staticMode)
                    allDependencies << package.requiresPrivate;

                for (const auto &dependency: allDependencies) {
                    const auto it = packageHash.find(dependency.name);
                    if (it == packageHash.end())
                        return makeMissingDependency(result, dependency);

                    const auto childEntry = f(*it->second, f);
                    if (childEntry.package.isBroken())
                        return makeBrokenDependency(result, dependency);

                    const auto &mergedPackage = childEntry.package.asPackage();
                    const bool versionOk = versionTest(
                            dependency.comparison, mergedPackage.version, dependency.version);
                    if (!versionOk)
                        return makeVersionMismatchDependency(result, mergedPackage, dependency);

                    for (const auto *dep: childEntry.deps) {
                        if (visitedDeps.insert(dep).second)
                            entry.deps.push_back(dep);
                    }
                }

                entry.deps.push_back(&package);

                for (const auto *dep: entry.deps) {
                    mergeFlags(result.libs, dep->libs);
                    mergeFlags(result.cflags, dep->cflags);
                }

                return result;
            }
            return package;
        };
        entry.package = package.visit(visitor);

        return entry;
    };

    for (const auto &package: packages)
        func(package, func);

    Packages result;
    for (auto &[key, value]: mergedHash)
        result.push_back(std::move(value.package));
    return result;
}

PkgConfig::Packages PkgConfig::findPackages() const
{
    Packages result;
    PcParser parser(*this);

    const auto systemLibraryPaths = !m_options.allowSystemLibraryPaths ?
                std::unordered_set<std::string>(
                m_options.systemLibraryPaths.begin(),
                m_options.systemLibraryPaths.end()) : std::unordered_set<std::string>();

    auto allSearchPaths = m_options.extraPaths;
    allSearchPaths.insert(
            allSearchPaths.end(), m_options.libDirs.begin(), m_options.libDirs.end());
    const auto pcFilePaths = getPcFilePaths(allSearchPaths);

    for (const auto &pcFilePath : pcFilePaths) {
        if (m_options.disableUninstalled) {
            if (pcFilePath.find("-uninstalled.pc") != std::string::npos)
                continue;
        }

        auto pkg = parser.parsePackageFile(pcFilePath);
        pkg.visit([&](auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PcPackage>) { // NOLINT
                value = std::move(value)
                        // Weird, but pkg-config removes libs first and only then appends
                        // sysroot. Looks like sysroot has to be used with
                        // allowSystemLibraryPaths: true
                        .removeSystemLibraryPaths(systemLibraryPaths)
                        .prependSysroot(m_options.sysroot);
            }
        });
        result.emplace_back(std::move(pkg));
    }

    if (m_options.mergeDependencies)
        result = mergeDependencies(result);

    const auto lessThanPackage = [](const PcPackageVariant &lhs, const PcPackageVariant &rhs)
    {
        return lhs.getBaseFileName() < rhs.getBaseFileName();
    };
    std::sort(result.begin(), result.end(), lessThanPackage);
    return result;
}

} // namespace qbs
