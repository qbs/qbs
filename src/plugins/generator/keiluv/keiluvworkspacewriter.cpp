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

#include "keiluvproperty.h"
#include "keiluvpropertygroup.h"
#include "keiluvworkspace.h"
#include "keiluvworkspacewriter.h"

#include <ostream>

namespace qbs {

KeiluvWorkspaceWriter::KeiluvWorkspaceWriter(std::ostream *device)
    : m_device(device)
{
    m_writer.reset(new QXmlStreamWriter(&m_buffer));
    m_writer->setAutoFormatting(true);
}

bool KeiluvWorkspaceWriter::write(const KeiluvWorkspace *workspace)
{
    m_buffer.clear();
    m_writer->writeStartDocument();
    workspace->accept(this);
    m_writer->writeEndDocument();
    if (m_writer->hasError())
        return false;
    m_device->write(&*std::begin(m_buffer), m_buffer.size());
    return m_device->good();
}

void KeiluvWorkspaceWriter::visitStart(const KeiluvWorkspace *workspace)
{
    Q_UNUSED(workspace)
    m_writer->writeStartElement(QStringLiteral("ProjectWorkspace"));
    m_writer->writeAttribute(QStringLiteral("xmlns:xsi"),
                             QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"));
    m_writer->writeAttribute(QStringLiteral("xsi:noNamespaceSchemaLocation"),
                             QStringLiteral("project_mpw.xsd"));
}

void KeiluvWorkspaceWriter::visitEnd(const KeiluvWorkspace *workspace)
{
    Q_UNUSED(workspace)
    m_writer->writeEndElement();
}

void KeiluvWorkspaceWriter::visitStart(const KeiluvProperty *property)
{
    const QString stringValue = property->value().toString();
    m_writer->writeTextElement(QString::fromLatin1(property->name()), stringValue);
}

void KeiluvWorkspaceWriter::visitEnd(const KeiluvProperty *property)
{
    Q_UNUSED(property)
}

void KeiluvWorkspaceWriter::visitStart(const KeiluvPropertyGroup *propertyGroup)
{
    m_writer->writeStartElement(QString::fromLatin1(propertyGroup->name()));
}

void KeiluvWorkspaceWriter::visitEnd(const KeiluvPropertyGroup *propertyGroup)
{
    Q_UNUSED(propertyGroup)
    m_writer->writeEndElement();
}

} // namespace qbs
