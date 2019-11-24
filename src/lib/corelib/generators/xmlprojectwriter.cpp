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

#include "xmlproject.h"
#include "xmlprojectwriter.h"
#include "xmlproperty.h"
#include "xmlpropertygroup.h"

#include <memory>
#include <ostream>

namespace qbs {
namespace gen {
namespace xml {

ProjectWriter::ProjectWriter(std::ostream *device)
    : m_device(device)
{
    m_writer = std::make_unique<QXmlStreamWriter>(&m_buffer);
    m_writer->setAutoFormatting(true);
}

bool ProjectWriter::write(const Project *project)
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

void ProjectWriter::visitPropertyStart(const Property *property)
{
    const auto value = property->value().toString();
    const auto name = QString::fromUtf8(property->name());
    m_writer->writeTextElement(name, value);
}

void ProjectWriter::visitPropertyEnd(const Property *property)
{
    Q_UNUSED(property)
}

void ProjectWriter::visitPropertyGroupStart(const PropertyGroup *propertyGroup)
{
    const auto name = QString::fromUtf8(propertyGroup->name());
    m_writer->writeStartElement(name);
}

void ProjectWriter::visitPropertyGroupEnd(const PropertyGroup *propertyGroup)
{
    Q_UNUSED(propertyGroup)
    m_writer->writeEndElement();
}

QXmlStreamWriter *ProjectWriter::writer() const
{
    return m_writer.get();
}

} // namespace xml
} // namespace gen
} // namespace qbs
