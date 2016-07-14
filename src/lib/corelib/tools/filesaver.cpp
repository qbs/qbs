/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "filesaver.h"

#include <QFile>
#include <QSaveFile>

namespace qbs {
namespace Internal {

FileSaver::FileSaver(const QString &filePath, bool overwriteIfUnchanged)
    : m_filePath(filePath), m_overwriteIfUnchanged(overwriteIfUnchanged)
{
}

QIODevice *FileSaver::device()
{
    return m_memoryDevice.data();
}

bool FileSaver::open()
{
    if (!m_overwriteIfUnchanged) {
        QFile file(m_filePath);
        if (file.open(QIODevice::ReadOnly))
            m_oldFileContents = file.readAll();
        else
            m_oldFileContents.clear();
    }

    m_newFileContents.clear();
    m_memoryDevice.reset(new QBuffer(&m_newFileContents));
    return m_memoryDevice->open(QIODevice::WriteOnly);
}

bool FileSaver::commit()
{
    if (!m_overwriteIfUnchanged && m_oldFileContents == m_newFileContents)
        return true; // no need to write unchanged data

    QSaveFile saveFile(m_filePath);
    if (!saveFile.open(QIODevice::WriteOnly))
        return false;

    saveFile.write(m_newFileContents);
    return saveFile.commit();
}

qint64 FileSaver::write(const QByteArray &data)
{
    return device()->write(data);
}

} // namespace Internal
} // namespace qbs
