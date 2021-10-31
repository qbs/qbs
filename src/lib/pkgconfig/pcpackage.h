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

#ifndef PC_PACKAGE_H
#define PC_PACKAGE_H

#include <variant.h>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qbs {

class PcPackage
{
public:
    struct Flag
    {
        enum class Type {
            LibraryName = (1 << 0),
            StaticLibraryName = (1 << 1),
            LibraryPath = (1 << 2),
            Framework = (1 << 3),
            FrameworkPath = (1 << 4),
            LinkerFlag = (1 << 5), // this is a lie, this is DriverLinkerFlags
            IncludePath = (1 << 6),
            SystemIncludePath = (1 << 7),
            DirAfterIncludePath = (1 << 8),
            Define = (1 << 9),
            CompilerFlag = (1 << 10),
        };
        Type type{Type::CompilerFlag};
        std::string value;

        static std::string_view typeToString(Type t);
        static std::optional<Type> typeFromString(std::string_view s);
    };

    struct RequiredVersion
    {
        enum class ComparisonType {
            LessThan,
            GreaterThan,
            LessThanEqual,
            GreaterThanEqual,
            Equal,
            NotEqual,
            AlwaysMatch
        };

        std::string name;
        ComparisonType comparison{ComparisonType::GreaterThanEqual};
        std::string version;

        static std::string_view comparisonToString(ComparisonType t);
        static std::optional<ComparisonType> comparisonFromString(std::string_view s);
    };

    std::string filePath;
    std::string baseFileName;
    std::string name;
    std::string version;
    std::string description;
    std::string url;

    std::vector<Flag> libs;
    std::vector<Flag> libsPrivate;
    std::vector<Flag> cflags;

    std::vector<RequiredVersion> requiresPublic;
    std::vector<RequiredVersion> requiresPrivate;
    std::vector<RequiredVersion> conflicts;

    using VariablesMap = std::map<std::string, std::string, std::less<>>;
    VariablesMap variables;

    bool uninstalled{false};

    PcPackage prependSysroot(std::string_view sysroot) &&;
    PcPackage removeSystemLibraryPaths(const std::unordered_set<std::string> &libraryPaths) &&;
};

class PcBrokenPackage
{
public:
    std::string filePath;
    std::string baseFileName;
    std::string errorText;
};

class PcPackageVariant: public Variant::variant<PcPackage, PcBrokenPackage>
{
public:
    using Base = Variant::variant<PcPackage, PcBrokenPackage>;
    using Base::Base;

    bool isValid() const noexcept { return index() == 0; }
    bool isBroken() const noexcept { return index() == 1; }

    const PcPackage &asPackage() const { return Variant::get<PcPackage>(*this); }
    PcPackage &asPackage() { return Variant::get<PcPackage>(*this); }

    const PcBrokenPackage &asBrokenPackage() const { return Variant::get<PcBrokenPackage>(*this); }
    PcBrokenPackage &asBrokenPackage() { return Variant::get<PcBrokenPackage>(*this); }

    template<typename F>
    decltype(auto) visit(F &&f) const
    {
         return Variant::visit(std::forward<F>(f), static_cast<const Base &>(*this));
    }

    template<typename F>
    decltype(auto) visit(F &&f)
    {
        return Variant::visit(std::forward<F>(f), static_cast<Base &>(*this));
    }

    const std::string &getBaseFileName() const
    {
        return visit([](auto &&value) noexcept -> const std::string & {
            return value.baseFileName;
        });
    }
};

class PcException: public std::runtime_error
{
public:
    explicit PcException(const std::string &message) : std::runtime_error(message) {}
};

inline bool operator==(const PcPackage::Flag &lhs, const PcPackage::Flag &rhs)
{
    return lhs.type == rhs.type && lhs.value == rhs.value;
}

inline bool operator!=(const PcPackage::Flag &lhs, const PcPackage::Flag &rhs)
{
    return !(lhs == rhs);
}

} // namespace qbs

namespace std {
template<> struct hash<qbs::PcPackage::Flag>
{
    size_t operator()(const qbs::PcPackage::Flag &v) const noexcept
    {
        return hash<std::string>()(v.value) + size_t(v.type);
    }
};
} // namespace std

#endif // PC_PACKAGE_H
