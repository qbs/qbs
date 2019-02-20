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

#ifndef QBS_IOSUTILS_H
#define QBS_IOSUTILS_H

#include <cstdio>
#include <cstring>
#include <ostream>

#if defined(_WIN32) && defined(_MSC_VER)
#include <codecvt>
#include <locale>
#define QBS_RENAME_IMPL ::_wrename
#define QBS_UNLINK_IMPL ::_wunlink
using qbs_filesystem_path_string_type = std::wstring;
#else
#include <unistd.h>
#define QBS_RENAME_IMPL ::rename
#define QBS_UNLINK_IMPL ::unlink
using qbs_filesystem_path_string_type = std::string;
#endif

namespace qbs {
namespace Internal {

static inline bool fwrite(const char *values, size_t nitems, std::ostream *stream)
{
    if (!stream)
        return false;
    stream->write(values, nitems);
    return stream->good();
}

template <class C>
bool fwrite(const C &container, std::ostream *stream)
{
    return fwrite(&*(std::begin(container)), container.size(), stream);
}

static inline bool fwrite(const char *s, std::ostream *stream)
{
    return fwrite(s, strlen(s), stream);
}

static inline qbs_filesystem_path_string_type utf8_to_native_path(const std::string &str)
{
#if defined(_WIN32) && defined(_MSC_VER)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
#else
    return str;
#endif
}

static inline int rename(const std::string &oldName, const std::string &newName)
{
    const auto wOldName = utf8_to_native_path(oldName);
    const auto wNewName = utf8_to_native_path(newName);
    return QBS_RENAME_IMPL(wOldName.c_str(), wNewName.c_str());
}

static inline int unlink(const std::string &name)
{
    const auto wName = utf8_to_native_path(name);
    return QBS_UNLINK_IMPL(wName.c_str());
}

} // namespace Internal
} // namespace qbs

#endif // QBS_IOSUTILS_H
