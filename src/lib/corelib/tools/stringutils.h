/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QBS_STRINGUTILS_H
#define QBS_STRINGUTILS_H

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

namespace qbs {
namespace Internal {

template <class C>
typename C::value_type join(const C &container, const typename C::value_type &separator)
{
    typename C::value_type out;
    if (!container.empty()) {
        auto it = container.cbegin();
        auto end = container.cend();
        out.append(*it++);
        for (; it != end; ++it) {
            out.append(separator);
            out.append(*it);
        }
    }
    return out;
}

template <class C>
typename C::value_type join(const C &container, typename C::value_type::value_type separator)
{
    typename C::value_type s;
    s.push_back(separator);
    return join(container, s);
}

static inline std::string trimmed(const std::string &s)
{
    // trim from start
    static const auto ltrim = [](std::string &s) -> std::string & {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                [](char c){ return !std::isspace(c); }));
        return s;
    };

    // trim from end
    static const auto rtrim = [](std::string &s) -> std::string & {
        s.erase(std::find_if(s.rbegin(), s.rend(),
                [](char c){ return !std::isspace(c); }).base(), s.end());
        return s;
    };

    // trim from both ends
    static const auto trim = [](std::string &s) -> std::string & {
        return ltrim(rtrim(s));
    };

    std::string copy = s;
    return trim(copy);
}

static inline bool startsWith(const std::string &subject, const std::string &s)
{
    if (s.size() <= subject.size())
        return std::equal(s.begin(), s.end(), subject.begin());
    return false;
}

static inline bool startsWith(const std::string &subject, char c)
{
    std::string s;
    s.push_back(c);
    return startsWith(subject, s);
}

static inline bool endsWith(const std::string &subject, const std::string &s)
{
    if (s.size() <= subject.size())
        return std::equal(s.rbegin(), s.rend(), subject.rbegin());
    return false;
}

static inline bool endsWith(const std::string &subject, char c)
{
    std::string s;
    s.push_back(c);
    return endsWith(subject, s);
}

} // namespace Internal
} // namespace qbs

#ifdef Q_DECLARE_METATYPE
Q_DECLARE_METATYPE(std::string)
#endif

#endif // QBS_STRINGUTILS_H
