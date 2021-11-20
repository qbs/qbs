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

#include "pcpackage.h"

#include <algorithm>

namespace qbs {

using ComparisonType = PcPackage::RequiredVersion::ComparisonType;

std::string_view PcPackage::Flag::typeToString(Type t)
{
    switch (t) {
    case Type::LibraryName: return "LibraryName";
    case Type::StaticLibraryName: return "StaticLibraryName";
    case Type::LibraryPath: return "LibraryPath";
    case Type::Framework: return "Framework";
    case Type::FrameworkPath: return "FrameworkPath";
    case Type::LinkerFlag: return "LinkerFlag";
    case Type::IncludePath: return "IncludePath";
    case Type::SystemIncludePath: return "SystemIncludePath";
    case Type::DirAfterIncludePath: return "DirAfterIncludePath";
    case Type::Define: return "Define";
    case Type::CompilerFlag: return "CompilerFlag";
    }
    return {};
}

std::optional<PcPackage::Flag::Type> PcPackage::Flag::typeFromString(std::string_view s)
{
    if (s == "LibraryName")
        return Type::LibraryName;
    if (s == "StaticLibraryName")
        return Type::StaticLibraryName;
    if (s == "LibraryPath")
        return Type::LibraryPath;
    if (s == "Framework")
        return Type::Framework;
    if (s == "FrameworkPath")
        return Type::FrameworkPath;
    if (s == "LinkerFlag")
        return Type::LinkerFlag;
    if (s == "IncludePath")
        return Type::IncludePath;
    if (s == "SystemIncludePath")
        return Type::SystemIncludePath;
    if (s == "DirAfterIncludePath")
        return Type::DirAfterIncludePath;
    if (s == "Define")
        return Type::Define;
    if (s == "CompilerFlag")
        return Type::CompilerFlag;
    return std::nullopt;
}

std::string_view PcPackage::RequiredVersion::comparisonToString(ComparisonType t)
{
    switch (t) {
    case ComparisonType::LessThan: return "LessThan";
    case ComparisonType::GreaterThan: return "GreaterThan";
    case ComparisonType::LessThanEqual: return "LessThanEqual";
    case ComparisonType::GreaterThanEqual: return "GreaterThanEqual";
    case ComparisonType::Equal: return "Equal";
    case ComparisonType::NotEqual: return "NotEqual";
    case ComparisonType::AlwaysMatch: return "AlwaysMatch";
    }
    return {};
}

std::optional<ComparisonType> PcPackage::RequiredVersion::comparisonFromString(std::string_view s)
{
    if (s == "LessThan")
        return ComparisonType::LessThan;
    if (s == "GreaterThan")
        return ComparisonType::GreaterThan;
    if (s == "LessThanEqual")
        return ComparisonType::LessThanEqual;
    if (s == "GreaterThanEqual")
        return ComparisonType::GreaterThanEqual;
    if (s == "Equal")
        return ComparisonType::Equal;
    if (s == "NotEqual")
        return ComparisonType::NotEqual;
    if (s == "AlwaysMatch")
        return ComparisonType::AlwaysMatch;
    return std::nullopt;
}

PcPackage PcPackage::prependSysroot(std::string_view sysroot) &&
{
    PcPackage package(std::move(*this));

    const auto doAppend = [](std::vector<Flag> flags, std::string_view sysroot)
    {
        if (sysroot.empty())
            return flags;
        for (auto &flag : flags) {
            if (flag.type == Flag::Type::IncludePath
                    || flag.type == Flag::Type::SystemIncludePath
                    || flag.type == Flag::Type::DirAfterIncludePath
                    || flag.type == Flag::Type::LibraryPath) {
                flag.value = std::string(sysroot) + std::move(flag.value);
            }
        }
        return flags;
    };

    package.libs = doAppend(std::move(package.libs), sysroot);
    package.libsPrivate = doAppend(std::move(package.libsPrivate), sysroot);
    package.cflags = doAppend(std::move(package.cflags), sysroot);
    return package;
}

PcPackage PcPackage::removeSystemLibraryPaths(
        const std::unordered_set<std::string> &libraryPaths) &&
{
    PcPackage package(std::move(*this));
    if (libraryPaths.empty())
        return package;

    const auto doRemove = [&libraryPaths](std::vector<Flag> flags)
    {
        const auto predicate = [&libraryPaths](const Flag &flag)
        {
            return flag.type == Flag::Type::LibraryPath && libraryPaths.count(flag.value);
        };
        flags.erase(std::remove_if(flags.begin(), flags.end(), predicate), flags.end());
        return flags;
    };
    package.libs = doRemove(package.libs);
    package.libsPrivate = doRemove(package.libsPrivate);
    return package;
}

} // namespace qbs
