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

#include "pcparser.h"

#include "pkgconfig.h"

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
#include <QFileInfo>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace qbs {

namespace {

bool readOneLine(std::ifstream &file, std::string &line)
{
    bool quoted = false;
    bool comment = false;
    int n_read = 0;

    line = {};

    while (true) {
        char c;
        file.get(c);
        const bool ok = file.good();

        if (!ok) {
            if (quoted)
                line += '\\';

            return n_read > 0;
        }
        n_read++;

        if (c == '\r') {
            n_read--;
            continue;
        }

        if (quoted) {
            quoted = false;

            switch (c) {
            case '#':
                line += '#';
                break;
            case '\n':
                break;
            default:
                line += '\\';
                line += c;
                break;
            }
        } else {
            switch (c) {
            case '#':
                comment = true;
                break;
            case '\\':
                if (!comment)
                    quoted = true;
                break;
            case '\n':
                return n_read > 0;
            default:
                if (!comment)
                    line += c;
                break;
            }
        }
    }

    return n_read > 0;
}

std::string_view trimmed(std::string_view str)
{
    const auto predicate = [](int c){ return std::isspace(c); };
    const auto left = std::find_if_not(str.begin(), str.end(), predicate);
    const auto right = std::find_if_not(str.rbegin(), str.rend(), predicate).base();
    if (right <= left)
        return {};
    return std::string_view(&*left, std::distance(left, right));
}

// based on https://opensource.apple.com/source/distcc/distcc-31.0.81/popt/poptparse.c.auto.html
std::optional<std::vector<std::string>> splitCommand(std::string_view s)
{
    std::vector<std::string> result;
    std::string arg;

    char quote = '\0';

    for (auto it = s.begin(), end = s.end(); it != end; ++it) {
        if (quote == *it) {
            quote = '\0';
        } else if (quote != '\0') {
            if (*it == '\\') {
                ++it;
                if (it == s.end())
                    return std::nullopt;

                if (*it != quote)
                    arg += '\\';
            }
            arg += *it;
        } else if (isspace(*it)) {
            if (!arg.empty()) {
                result.push_back(arg);
                arg.clear();
            }
        } else {
            switch (*it) {
            case '"':
            case '\'':
                quote = *it;
                break;
            case '\\':
                ++it;
                if (it == s.end())
                    return std::nullopt;
                [[fallthrough]];
            default:
                arg += *it;
                break;
            }
        }
    }

    if (!arg.empty())
        result.push_back(arg);

    return result;
}

bool startsWith(std::string_view haystack, std::string_view needle)
{
    return haystack.size() >= needle.size() && haystack.compare(0, needle.size(), needle) == 0;
}

bool endsWith(std::string_view haystack, std::string_view needle)
{
    return haystack.size() >= needle.size()
            && haystack.compare(haystack.size() - needle.size(), needle.size(), needle) == 0;
}

[[noreturn]] void raizeUnknownComparisonException(const PcPackage &pkg, std::string_view verName, std::string_view comp)
{
    std::string message;
    message += "Unknown version comparison operator '";
    message += comp;
    message += "' after package name '";
    message += verName;
    message += "' in file '";
    message += pkg.filePath;
    message += "'";
    throw PcException(message);
}

[[noreturn]] void raiseDuplicateFieldException(std::string_view fieldName, std::string_view path)
{
    std::string message;
    message += fieldName;
    message += " field occurs twice in '";
    message += path;
    message += "'";
    throw PcException(message);
}

[[noreturn]] void raizeEmptyPackageNameException(const PcPackage &pkg)
{
    std::string message;
    message += "Empty package name in Requires or Conflicts in file '";
    message += pkg.filePath;
    message += "'";
    throw PcException(message);
}

[[noreturn]] void raizeNoVersionException(const PcPackage &pkg, std::string_view verName)
{
    std::string message;
    message += "Comparison operator but no version after package name '";
    message += verName;
    message += "' in file '";
    message += pkg.filePath;
    message += "'";
    throw PcException(message);
}

[[noreturn]] void raizeDuplicateVariableException(const PcPackage &pkg, std::string_view variable)
{
    std::string message;
    message += "Duplicate definition of variable '";
    message += variable;
    message += "' in '";
    message += pkg.filePath;
    throw PcException(message);
}

[[noreturn]] void raizeUndefinedVariableException(const PcPackage &pkg, std::string_view variable)
{
    std::string message;
    message += "Variable '";
    message += variable;
    message += "' not defined in '";
    message += pkg.filePath;
    throw PcException(message);
}

bool isModuleSeparator(char c) { return c == ',' || std::isspace(c); }
bool isModuleOperator(char c) { return c == '<' || c == '>' || c == '!' || c == '='; }

// A module list is a list of modules with optional version specification,
// separated by commas and/or spaces. Commas are treated just like whitespace,
// in order to allow stuff like: Requires: @FRIBIDI_PC@, glib, gmodule
// where @FRIBIDI_PC@ gets substituted to nothing or to 'fribidi'

std::vector<std::string> splitModuleList(std::string_view str)
{
    enum class State {
      // put numbers to help interpret lame debug spew ;-)
      OutsideModule = 0,
      InModuleName = 1,
      BeforeOperator = 2,
      InOperator = 3,
      AfterOperator = 4,
      InModuleVersion = 5
    };

    std::vector<std::string> result;
    State state = State::OutsideModule;
    State last_state = State::OutsideModule;

    auto start = str.begin();
    const auto end = str.end();
    auto p = start;

    while (p != end) {

        switch (state) {
        case State::OutsideModule:
            if (!isModuleSeparator(*p))
                state = State::InModuleName;
            break;

        case State::InModuleName:
            if (std::isspace(*p)) {
                // Need to look ahead to determine next state
                auto s = p;
                while (s != end && std::isspace (*s))
                    ++s;

                state = State::OutsideModule;
                if (s != end && isModuleOperator(*s))
                    state = State::BeforeOperator;
            }
            else if (isModuleSeparator(*p))
                state = State::OutsideModule; // comma precludes any operators
            break;

        case State::BeforeOperator:
            // We know an operator is coming up here due to lookahead from
            // IN_MODULE_NAME
            if (std::isspace(*p))
                ; // no change
            else if (isModuleOperator(*p))
                state = State::InOperator;
            break;

        case State::InOperator:
            if (!isModuleOperator(*p))
                state = State::AfterOperator;
            break;

        case State::AfterOperator:
            if (!std::isspace(*p))
                state = State::InModuleVersion;
            break;

        case State::InModuleVersion:
            if (isModuleSeparator(*p))
                state = State::OutsideModule;
            break;

        default:
            break;
        }

        if (state == State::OutsideModule && last_state != State::OutsideModule) {
            // We left a module
            while (start != end && isModuleSeparator(*start))
                ++start;

            std::string module(&*start, p - start);
            result.push_back(module);

            // reset start
            start = p;
        }

        last_state = state;
        ++p;
    }

    if (p != start) {
        // get the last module
        while (start != end && isModuleSeparator(*start))
            ++start;
        std::string module(&*start, p - start);
        result.push_back(module);
    }

    return result;
}

PcPackage::RequiredVersion::ComparisonType comparisonFromString(
        const PcPackage &pkg, std::string_view verName, std::string_view comp)
{
    using ComparisonType = PcPackage::RequiredVersion::ComparisonType;
    if (comp.empty())
        return ComparisonType::AlwaysMatch;
    if (comp == "=")
        return ComparisonType::Equal;
    if (comp ==  ">=")
        return ComparisonType::GreaterThanEqual;
    if (comp == "<=")
        return ComparisonType::LessThanEqual;
    if (comp == ">")
        return ComparisonType::GreaterThan;
    if (comp == "<")
        return ComparisonType::LessThan;
    if (comp == "!=")
        return ComparisonType::NotEqual;

    raizeUnknownComparisonException(pkg, verName, comp);
}

std::string baseName(const std::string_view &filePath)
{
    auto pos = filePath.rfind('/');
    const auto fileName =
            pos == std::string_view::npos ? std::string_view() : filePath.substr(pos + 1);
    pos = fileName.rfind('.');
    return std::string(pos == std::string_view::npos
            ? std::string_view()
            : fileName.substr(0, pos));
}

} // namespace

PcParser::PcParser(const PkgConfig &pkgConfig)
    : m_pkgConfig(pkgConfig)
{

}

PcPackageVariant PcParser::parsePackageFile(const std::string &path)
try
{
    PcPackage package;

    if (path.empty())
        return package;

    std::ifstream file(path);

    if (!file.is_open())
        throw PcException(std::string("Can't open file ") + path);

    package.baseFileName = baseName(path);
#if HAS_STD_FILESYSTEM
    const auto fsPath = std::filesystem::path(path);
    package.filePath = fsPath.generic_string();
    package.variables["pcfiledir"] = fsPath.parent_path().generic_string();
#else
    QFileInfo fileInfo(QString::fromStdString(path));
    package.filePath = fileInfo.absoluteFilePath().toStdString();
    package.variables["pcfiledir"] = fileInfo.absolutePath().toStdString();
#endif

    std::string line;
    while (readOneLine(file, line))
        parseLine(package, line);
    return package;
} catch(const PcException &ex) {
    return PcBrokenPackage{path, baseName(path), ex.what()};
}

std::string PcParser::trimAndSubstitute(const PcPackage &pkg, std::string_view str) const
{
    str = trimmed(str);

    std::string result;

    while (!str.empty()) {
        if (startsWith(str, "$$")) {
            // escaped $
            result += '$';
            str.remove_prefix(2); // cut "$$"
        } else if (startsWith(str, "${")) {
            // variable
            str.remove_prefix(2); // cut "${"
            const auto it = std::find(str.begin(), str.end(), '}');
            // funny, original pkg-config simply reads all available memory here
            if (it == str.end())
                throw PcException("Missing closing '}'");

            const std::string_view varname = str.substr(0, std::distance(str.begin(), it));

            // past brace
            str.remove_prefix(varname.size());
            str.remove_prefix(1);

            const auto varval = m_pkgConfig.packageGetVariable(pkg, varname);

            if (varval.empty())
                raizeUndefinedVariableException(pkg, varname);

            result += varval;
        } else {
            result += str.front();
            str.remove_prefix(1);
        }
    }

    return result;
}

void PcParser::parseStringField(
        PcPackage &pkg,
        std::string &field,
        std::string_view fieldName,
        std::string_view str)
{
    if (!field.empty())
        raiseDuplicateFieldException(fieldName, pkg.filePath);

    field = trimAndSubstitute(pkg, str);
}

void PcParser::parseLibs(
        PcPackage &pkg,
        std::vector<PcPackage::Flag> &libs,
        std::string_view fieldName,
        std::string_view str)
{
    // Strip out -l and -L flags, put them in a separate list.

    if (!libs.empty())
        raiseDuplicateFieldException(fieldName, pkg.filePath);

    const auto trimmed = trimAndSubstitute(pkg, str);

    const auto argv = splitCommand(trimmed);
    if (!trimmed.empty() && !argv)
        throw PcException("Couldn't parse Libs field into an argument vector");

    libs = doParseLibs(*argv);
}

std::vector<PcPackage::Flag> PcParser::doParseLibs(const std::vector<std::string> &argv)
{
    std::vector<PcPackage::Flag> libs;
    libs.reserve(argv.size());

    for (auto it = argv.begin(), end = argv.end(); it != end; ++it) {
        PcPackage::Flag flag;
        const auto escapedArgument = trimmed(*it);
        std::string_view arg(escapedArgument);

        // -lib: is used by the C# compiler for libs; it's not an -l flag.
        if (startsWith(arg, "-l") && !startsWith(arg, "-lib:")) {
            arg.remove_prefix(2);
            arg = trimmed(arg);

            flag.type = PcPackage::Flag::Type::LibraryName;
            flag.value += arg;
        } else if (startsWith(arg, "-L")) {
            arg.remove_prefix(2);
            arg = trimmed(arg);

            flag.type = PcPackage::Flag::Type::LibraryPath;
            flag.value += arg;
        } else if ((arg == "-framework" /*|| arg == "-Wl,-framework"*/) && it + 1 != end) {
            // macOS has a -framework Foo which is really one option,
            // so we join those to avoid having -framework Foo
            // -framework Bar being changed into -framework Foo Bar
            // later
            const auto framework = trimmed(*(it + 1));
            flag.type = PcPackage::Flag::Type::Framework;
            flag.value += framework;
            ++it;
        } else if (startsWith(arg, "-F")) {
            arg.remove_prefix(2);
            arg = trimmed(arg);

            flag.type = PcPackage::Flag::Type::FrameworkPath;
            flag.value += arg;

        } else if (!startsWith(arg, "-") && (endsWith(arg, ".a") || endsWith(arg, ".lib"))) {
            flag.type = PcPackage::Flag::Type::StaticLibraryName;
            flag.value += arg;
        } else if (!arg.empty()) {
            flag.type = PcPackage::Flag::Type::LinkerFlag;
            flag.value += arg;
        } else {
            continue;
        }
        libs.push_back(flag);
    }
    return libs;
}

void PcParser::parseCFlags(PcPackage &pkg, std::string_view str)
{
    // Strip out -I, -D, -isystem and idirafter flags, put them in a separate lists.

    if (!pkg.cflags.empty())
        raiseDuplicateFieldException("Cflags", pkg.filePath);

    const auto command = trimAndSubstitute(pkg, str);

    const auto argv = splitCommand(command);
    if (!command.empty() && !argv)
        throw PcException("Couldn't parse Cflags field into an argument vector");

    std::vector<PcPackage::Flag> cflags;
    cflags.reserve(argv->size());

    for (auto it = argv->begin(), end = argv->end(); it != end; ++it) {
        PcPackage::Flag flag;
        const auto escapedArgument = trimmed(*it);
        std::string_view arg(escapedArgument);

        if (startsWith(arg, "-I")) {
            arg.remove_prefix(2);
            arg = trimmed(arg);

            flag.type = PcPackage::Flag::Type::IncludePath;
            flag.value += arg;
        } else if (startsWith(arg, "-D")) {
            arg.remove_prefix(2);
            arg = trimmed(arg);

            flag.type = PcPackage::Flag::Type::Define;
            flag.value += arg;
        } else if (arg == "-isystem" && it + 1 != end) {
            flag.type = PcPackage::Flag::Type::SystemIncludePath;
            flag.value = trimmed(*(it + 1));
            ++it;
        } else if (arg == "-idirafter" && it + 1 != end) {
            flag.type = PcPackage::Flag::Type::DirAfterIncludePath;
            flag.value = trimmed(*(it + 1));
            ++it;
        } else if (!arg.empty()) {
            flag.type = PcPackage::Flag::Type::CompilerFlag;
            flag.value += arg;
        } else {
            continue;
        }
        cflags.push_back(flag);
    }
    pkg.cflags = std::move(cflags);
}

std::vector<PcPackage::RequiredVersion> PcParser::parseModuleList(PcPackage &pkg, std::string_view str)
{
    using ComparisonType = PcPackage::RequiredVersion::ComparisonType;

    std::vector<PcPackage::RequiredVersion> result;
    auto split = splitModuleList(str);

    for (auto &module: split) {
        PcPackage::RequiredVersion ver;

        auto p = module.begin();
        const auto end = module.end();

        ver.comparison = ComparisonType::AlwaysMatch;

        auto start = p;

        while (*p && !std::isspace(*p))
            ++p;

        const auto name = std::string_view(&*start, std::distance(start, p));

        if (name.empty())
            raizeEmptyPackageNameException(pkg);

        ver.name = std::string(name);

        while (p != end && std::isspace(*p))
            ++p;

        start = p;

        while (p != end && !std::isspace(*p))
            ++p;

        const auto comp = std::string_view(&*start, std::distance(start, p));
        ver.comparison = comparisonFromString(pkg, ver.name, comp);

        while (p != end && std::isspace(*p))
            ++p;

        start = p;

        while (p != end && !std::isspace(*p))
            ++p;

        const auto version = std::string_view(&*start, std::distance(start, p));

        while (p != end && std::isspace(*p))
            ++p;

        if (ver.comparison != ComparisonType::AlwaysMatch && version.empty())
            raizeNoVersionException(pkg, ver.name);

        ver.version = std::string(version);

        result.push_back(ver);
    }

    return result;
}

void PcParser::parseVersionsField(
        PcPackage &pkg,
        std::vector<PcPackage::RequiredVersion> &modules,
        std::string_view fieldName,
        std::string_view str)
{
    if (!modules.empty())
        raiseDuplicateFieldException(fieldName, pkg.filePath);

    const auto trimmed = trimAndSubstitute(pkg, str);
    modules = parseModuleList(pkg, trimmed.c_str());
}

void PcParser::parseLine(PcPackage &pkg, std::string_view str)
{
    str = trimmed(str);
    if (str.empty())
        return;

    auto getFirstWord = [](std::string_view s) {
        size_t pos = 0;
        for (; pos < s.size(); ++pos) {
            auto p = s.data() + pos;
            if (!((*p >= 'A' && *p <= 'Z') ||
                   (*p >= 'a' && *p <= 'z') ||
                   (*p >= '0' && *p <= '9') ||
                   *p == '_' || *p == '.')) {
                break;
            }
        }
        return s.substr(0, pos);
    };

    const auto tag = getFirstWord(str);

    str.remove_prefix(tag.size()); // cut tag
    str = trimmed(str);

    if (str.empty())
        return;

    if (str.front() == ':') {
        // keyword
        str.remove_prefix(1); // cut ':'
        str = trimmed(str);

        if (tag == "Name")
            parseStringField(pkg, pkg.name, tag, str);
        else if (tag == "Description")
            parseStringField(pkg, pkg.description, tag, str);
        else if (tag == "Version")
            parseStringField(pkg, pkg.version, tag, str);
        else if (tag == "Requires.private")
            parseVersionsField(pkg, pkg.requiresPrivate, tag, str);
        else if (tag == "Requires")
            parseVersionsField(pkg, pkg.requiresPublic, tag, str);
        else if (tag == "Libs.private")
            parseLibs(pkg, pkg.libsPrivate, "Libs.private", str);
        else if (tag == "Libs")
            parseLibs(pkg, pkg.libs, "Libs", str);
        else if (tag == "Cflags" || tag == "CFlags")
            parseCFlags(pkg, str);
        else if (tag == "Conflicts")
            parseVersionsField(pkg, pkg.conflicts, tag, str);
        else if (tag == "URL")
            parseStringField(pkg, pkg.url, tag, str);
        else {
            // we don't error out on unknown keywords because they may
            // represent additions to the .pc file format from future
            // versions of pkg-config.
            return;
        }
    } else if (str.front() == '=') {
        // variable

        str.remove_prefix(1); // cut '='
        str = trimmed(str);

        // TODO: support guesstimating of the prefix variable (pkg-config's --define-prefix option)
        // from doc: "try to override the value of prefix for each .pc file found with a
        // guesstimated value based on the location of the .pc file"
        // https://gitlab.freedesktop.org/pkg-config/pkg-config/-/blob/pkg-config-0.29.2/parse.c#L998
        // This option is disabled by default, and Qbs doesn't allow to override it yet, so we can
        // ignore this feature for now

        const auto value = trimAndSubstitute(pkg, str);
        if (!pkg.variables.insert({std::string(tag), value}).second)
            raizeDuplicateVariableException(pkg, tag);
    }
}

} // namespace qbs
