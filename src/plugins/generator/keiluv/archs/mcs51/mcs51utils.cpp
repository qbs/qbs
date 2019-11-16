/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "mcs51utils.h"

namespace qbs {
namespace keiluv {
namespace mcs51 {

namespace KeiluvUtils {

static QString extractValue(const QString &flag)
{
    const auto openBracketIndex = flag.indexOf(QLatin1Char('('));
    const auto closeBracketIndex = flag.indexOf(QLatin1Char(')'));
    const auto n = closeBracketIndex - openBracketIndex - 1;
    return flag.mid(openBracketIndex + 1, n);
}

QStringList flagValues(const QStringList &flags, const QString &flagKey)
{
    QStringList values;
    for (const auto &flag : flags) {
        if (!flag.startsWith(flagKey, Qt::CaseInsensitive))
            continue;
        const auto value = extractValue(flag);
        values.push_back(value);
    }
    return values;
}

QString flagValue(const QStringList &flags, const QString &flagKey)
{
    const auto flagEnd = flags.cend();
    const auto flagIt = std::find_if(flags.cbegin(), flagEnd,
                                     [flagKey](const auto &flag) {
        return flag.startsWith(flagKey, Qt::CaseInsensitive);
    });
    if (flagIt == flagEnd)
        return {}; // Flag key not found.
    return extractValue(*flagIt);
}

QStringList flagValueParts(const QString &flagValue, const QLatin1Char &sep)
{
    auto parts = flagValue.split(sep);
    std::transform(parts.begin(), parts.end(), parts.begin(),
                   [](const auto &part) { return part.trimmed(); });
    return parts;
}

} // namespace KeiluvUtils

} // namespace mcs51
} // namespace keiluv
} // namespace qbs
