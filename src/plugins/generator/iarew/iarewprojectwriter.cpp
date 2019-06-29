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

#include "iarewproject.h"
#include "iarewprojectwriter.h"
#include "iarewproperty.h"
#include "iarewpropertygroup.h"

#include <ostream>

namespace qbs {

IarewProjectWriter::IarewProjectWriter(std::ostream *device)
    : m_device(device)
{
    m_writer.reset(new QXmlStreamWriter(&m_buffer));
    m_writer->setAutoFormatting(true);
}

bool IarewProjectWriter::write(const IarewProject *project)
{
    m_buffer.clear();
    m_writer->writeStartDocument();
    project->accept(this);
    m_writer->writeEndDocument();
    if (m_writer->hasError())
        return false;
    m_device->write(&*std::begin(m_buffer), m_buffer.size());
    return m_device->good();
}

void IarewProjectWriter::visitStart(const IarewProject *project)
{
    Q_UNUSED(project)
    m_writer->writeStartElement(QStringLiteral("project"));
}

void IarewProjectWriter::visitEnd(const IarewProject *project)
{
    Q_UNUSED(project)
    m_writer->writeEndElement();
}

void IarewProjectWriter::visitStart(const IarewProperty *property)
{
    const QString stringValue = property->value().toString();
    m_writer->writeTextElement(QString::fromUtf8(property->name()), stringValue);
}

void IarewProjectWriter::visitEnd(const IarewProperty *property)
{
    Q_UNUSED(property)
}

void IarewProjectWriter::visitStart(const IarewPropertyGroup *propertyGroup)
{
    m_writer->writeStartElement(QString::fromUtf8(propertyGroup->name()));
}

void IarewProjectWriter::visitEnd(const IarewPropertyGroup *propertyGroup)
{
    Q_UNUSED(propertyGroup)
    m_writer->writeEndElement();
}

} // namespace qbs
