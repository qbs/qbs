/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "importversion.h"
#include <logging/translator.h>
#include <tools/error.h>
#include <QStringList>

namespace qbs {
namespace Internal {

ImportVersion::ImportVersion()
    : m_major(0), m_minor(0)
{
}

ImportVersion ImportVersion::fromString(const QString &str, const CodeLocation &location)
{
    QStringList lst = str.split(QLatin1Char('.'));
    if (Q_UNLIKELY(lst.count() < 1 || lst.count() > 2))
        throw ErrorInfo(Tr::tr("Wrong number of components in import version."), location);
    ImportVersion v;
    int *parts[] = {&v.m_major, &v.m_minor, 0};
    for (int i = 0; i < lst.count(); ++i) {
        if (!parts[i])
            break;
        bool ok;
        *parts[i] = lst.at(i).toInt(&ok);
        if (Q_UNLIKELY(!ok))
            throw ErrorInfo(Tr::tr("Cannot parse import version."), location);
    }
    return v;
}

bool ImportVersion::operator <(const ImportVersion &rhs) const
{
    return m_major < rhs.m_major || (m_major == rhs.m_major && m_minor < rhs.m_minor);
}

bool ImportVersion::operator ==(const ImportVersion &rhs) const
{
    return m_major == rhs.m_major && m_minor == rhs.m_minor;
}

bool ImportVersion::operator !=(const ImportVersion &rhs) const
{
    return !operator ==(rhs);
}

} // namespace Internal
} // namespace qbs
