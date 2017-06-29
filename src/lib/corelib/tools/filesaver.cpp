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

#include "filesaver.h"
#include "stlutils.h"

#include <QtCore/qfile.h>
#include <QtCore/qsavefile.h>

#include <tools/iosutils.h>

#include <fstream>

namespace qbs {
namespace Internal {

FileSaver::FileSaver(const std::string &filePath, bool overwriteIfUnchanged)
    : m_filePath(filePath), m_overwriteIfUnchanged(overwriteIfUnchanged)
{
}

std::ostream *FileSaver::device()
{
    return m_memoryDevice.get();
}

bool FileSaver::open()
{
    if (!m_overwriteIfUnchanged) {
        std::ifstream file(utf8_to_native_path(m_filePath));
        if (file.is_open())
            m_oldFileContents.assign(std::istreambuf_iterator<std::ifstream::char_type>(file),
                                     std::istreambuf_iterator<std::ifstream::char_type>());
        else
            m_oldFileContents.clear();
    }

    m_newFileContents.clear();
    m_memoryDevice = std::make_shared<std::ostringstream>(m_newFileContents);
    return true;
}

bool FileSaver::commit()
{
    if (!m_overwriteIfUnchanged && m_oldFileContents == m_newFileContents)
        return true; // no need to write unchanged data

    const std::string tempFilePath = std::tmpnam(nullptr);
    std::ofstream tempFile(utf8_to_native_path(tempFilePath));
    if (!tempFile.is_open())
        return false;

    tempFile.write(m_newFileContents.data(), m_newFileContents.size());
    if (!tempFile.good())
        return false;

    return Internal::rename(tempFilePath, m_filePath) == 0;
}

size_t FileSaver::write(const std::vector<char> &data)
{
    return fwrite(data, device());
}

size_t FileSaver::write(const std::string &data)
{
    return fwrite(data, device());
}

} // namespace Internal
} // namespace qbs
