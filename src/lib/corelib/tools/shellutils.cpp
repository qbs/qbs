/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#include "shellutils.h"

#include "pathutils.h"
#include "qttools.h"

#include <QtCore/qfile.h>
#include <QtCore/qregexp.h>
#include <QtCore/qtextstream.h>

namespace qbs {
namespace Internal {

QString shellInterpreter(const QString &filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream ts(&file);
        const QString shebang = ts.readLine();
        if (shebang.startsWith(QLatin1String("#!"))) {
            return (shebang.mid(2).split(QRegExp(QStringLiteral("\\s")),
                                         QBS_SKIP_EMPTY_PARTS) << QString()).front();
        }
    }

    return {};
}

// isSpecialChar, hasSpecialChars, shellQuoteUnix, shellQuoteWin:
// all from qtbase/qmake/library/ioutils.cpp

inline static bool isSpecialChar(ushort c, const uchar (&iqm)[16])
{
    return (c < sizeof(iqm) * 8) && (iqm[c / 8] & (1 << (c & 7)));
}

inline static bool hasSpecialChars(const QString &arg, const uchar (&iqm)[16])
{
    for (auto it = arg.crbegin(), end = arg.crend(); it != end; ++it) {
        if (isSpecialChar(it->unicode(), iqm))
            return true;
    }
    return false;
}

static QString shellQuoteUnix(const QString &arg)
{
    // Chars that should be quoted (TM). This includes:
    static const uchar iqm[] = {
        0xff, 0xff, 0xff, 0xff, 0xdf, 0x07, 0x00, 0xd8,
        0x00, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x78
    }; // 0-32 \'"$`<>|;&(){}*?#!~[]

    if (!arg.length())
        return QStringLiteral("''");

    QString ret(arg);
    if (hasSpecialChars(ret, iqm)) {
        ret.replace(QLatin1Char('\''), QLatin1String("'\\''"));
        ret.prepend(QLatin1Char('\''));
        ret.append(QLatin1Char('\''));
    }
    return ret;
}

static QString shellQuoteWin(const QString &arg)
{
    // Chars that should be quoted (TM). This includes:
    // - control chars & space
    // - the shell meta chars "&()<>^|
    // - the potential separators ,;=
    static const uchar iqm[] = {
        0xff, 0xff, 0xff, 0xff, 0x45, 0x13, 0x00, 0x78,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x10
    };
    // Shell meta chars that need escaping.
    static const uchar ism[] = {
        0x00, 0x00, 0x00, 0x00, 0x40, 0x03, 0x00, 0x50,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x10
    }; // &()<>^|

    if (!arg.length())
        return QStringLiteral("\"\"");

    QString ret(arg);
    if (hasSpecialChars(ret, iqm)) {
        // The process-level standard quoting allows escaping quotes with backslashes (note
        // that backslashes don't escape themselves, unless they are followed by a quote).
        // Consequently, quotes are escaped and their preceding backslashes are doubled.
        ret.replace(QRegExp(QLatin1String("(\\\\*)\"")), QLatin1String("\\1\\1\\\""));
        // Trailing backslashes must be doubled as well, as they are followed by a quote.
        ret.replace(QRegExp(QLatin1String("(\\\\+)$")), QLatin1String("\\1\\1"));
        // However, the shell also interprets the command, and no backslash-escaping exists
        // there - a quote always toggles the quoting state, but is nonetheless passed down
        // to the called process verbatim. In the unquoted state, the circumflex escapes
        // meta chars (including itself and quotes), and is removed from the command.
        bool quoted = true;
        for (int i = 0; i < ret.length(); i++) {
            QChar c = ret.unicode()[i];
            if (c.unicode() == '"')
                quoted = !quoted;
            else if (!quoted && isSpecialChar(c.unicode(), ism))
                ret.insert(i++, QLatin1Char('^'));
        }
        if (!quoted)
            ret.append(QLatin1Char('^'));
        ret.append(QLatin1Char('"'));
        ret.prepend(QLatin1Char('"'));
    }
    return ret;
}

QString shellQuote(const QString &arg, HostOsInfo::HostOs os)
{
    return os == HostOsInfo::HostOsWindows ? shellQuoteWin(arg) : shellQuoteUnix(arg);
}

std::string shellQuote(const std::string &arg, HostOsInfo::HostOs os)
{
    return shellQuote(QString::fromStdString(arg), os).toStdString();
}

QString shellQuote(const QStringList &args, HostOsInfo::HostOs os)
{
    QString result;
    if (!args.empty()) {
        result += shellQuote(args.at(0), os);
        for (int i = 1; i < args.size(); ++i)
            result += QLatin1Char(' ') + shellQuote(args.at(i), os);
    }
    return result;
}

std::string shellQuote(const std::vector<std::string> &args, HostOsInfo::HostOs os)
{
    std::string result;
    if (!args.empty()) {
        auto it = args.cbegin();
        const auto end = args.cend();
        result += shellQuote(*it++, os);
        for (; it != end; ++it) {
            result.push_back(' ');
            result.append(shellQuote(*it, os));
        }
    }
    return result;
}

QString shellQuote(const QString &program, const QStringList &args, HostOsInfo::HostOs os)
{
    QString result = shellQuote(program, os);
    if (!args.empty())
        result += QLatin1Char(' ') + shellQuote(args, os);
    return result;
}

void CommandLine::setProgram(const QString &program, bool raw)
{
    m_program = program;
    m_isRawProgram = raw;
}

void CommandLine::setProgram(const std::string &program, bool raw)
{
    m_program = QString::fromStdString(program);
    m_isRawProgram = raw;
}

void CommandLine::appendArgument(const QString &value)
{
    m_arguments.emplace_back(value);
}

void CommandLine::appendArgument(const std::string &value)
{
    m_arguments.emplace_back(QString::fromStdString(value));
}

void CommandLine::appendArguments(const QList<QString> &args)
{
    for (const QString &arg : args)
        appendArgument(arg);
}

void CommandLine::appendRawArgument(const QString &value)
{
    Argument arg(value);
    arg.shouldQuote = false;
    m_arguments.push_back(arg);
}

void CommandLine::appendRawArgument(const std::string &value)
{
    appendRawArgument(QString::fromStdString(value));
}

void CommandLine::appendPathArgument(const QString &value)
{
    Argument arg(value);
    arg.isFilePath = true;
    m_arguments.push_back(arg);
}

void CommandLine::clearArguments()
{
    m_arguments.clear();
}

QString CommandLine::toCommandLine(HostOsInfo::HostOs os) const
{
    QString result = PathUtils::toNativeSeparators(m_program, os);
    if (!m_isRawProgram)
        result = shellQuote(result, os);
    for (const Argument &arg : m_arguments) {
        const QString value = arg.isFilePath
                ? PathUtils::toNativeSeparators(arg.value, os)
                : arg.value;
        result += QLatin1Char(' ') + (arg.shouldQuote ? shellQuote(value, os) : value);
    }
    return result;
}

} // namespace Internal
} // namespace qbs
