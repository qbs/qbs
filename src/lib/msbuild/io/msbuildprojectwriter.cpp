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

#include "msbuildprojectwriter.h"

#include "../msbuild/imsbuildnodevisitor.h"
#include "../msbuild/msbuildimport.h"
#include "../msbuild/msbuildimportgroup.h"
#include "../msbuild/msbuilditem.h"
#include "../msbuild/msbuilditemdefinitiongroup.h"
#include "../msbuild/msbuilditemgroup.h"
#include "../msbuild/msbuilditemmetadata.h"
#include "../msbuild/msbuildproject.h"
#include "../msbuild/msbuildproperty.h"
#include "../msbuild/msbuildpropertygroup.h"

#include <QtCore/qxmlstream.h>

#include <memory>

namespace qbs {

static const QString kMSBuildSchemaURI =
        QStringLiteral("http://schemas.microsoft.com/developer/msbuild/2003");

class MSBuildProjectWriterPrivate : public IMSBuildNodeVisitor
{
public:
    std::ostream *device = nullptr;
    QByteArray buffer;
    std::unique_ptr<QXmlStreamWriter> writer;

    void visitStart(const MSBuildImport *import) override;
    void visitEnd(const MSBuildImport *import) override;

    void visitStart(const MSBuildImportGroup *importGroup) override;
    void visitEnd(const MSBuildImportGroup *importGroup) override;

    void visitStart(const MSBuildItem *item) override;
    void visitEnd(const MSBuildItem *item) override;

    void visitStart(const MSBuildItemDefinitionGroup *itemDefinitionGroup) override;
    void visitEnd(const MSBuildItemDefinitionGroup *itemDefinitionGroup) override;

    void visitStart(const MSBuildItemGroup *itemGroup) override;
    void visitEnd(const MSBuildItemGroup *itemGroup) override;

    void visitStart(const MSBuildItemMetadata *itemMetadata) override;
    void visitEnd(const MSBuildItemMetadata *itemMetadata) override;

    void visitStart(const MSBuildProject *project) override;
    void visitEnd(const MSBuildProject *project) override;

    void visitStart(const MSBuildProperty *property) override;
    void visitEnd(const MSBuildProperty *property) override;

    void visitStart(const MSBuildPropertyGroup *propertyGroup) override;
    void visitEnd(const MSBuildPropertyGroup *propertyGroup) override;
};

MSBuildProjectWriter::MSBuildProjectWriter(std::ostream *device)
    : d(new MSBuildProjectWriterPrivate)
{
    d->device = device;
    d->writer = std::make_unique<QXmlStreamWriter>(&d->buffer);
    d->writer->setAutoFormatting(true);
}

MSBuildProjectWriter::~MSBuildProjectWriter()
{
    delete d;
}

bool MSBuildProjectWriter::write(const MSBuildProject *project)
{
    d->buffer.clear();
    d->writer->writeStartDocument();
    project->accept(d);
    d->writer->writeEndDocument();
    if (d->writer->hasError())
        return false;
    d->device->write(&*std::begin(d->buffer), d->buffer.size());
    return d->device->good();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildImport *import)
{
    writer->writeStartElement(QStringLiteral("Import"));
    writer->writeAttribute(QStringLiteral("Project"), import->project());
    if (!import->condition().isEmpty())
        writer->writeAttribute(QStringLiteral("Condition"), import->condition());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildImport *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildImportGroup *importGroup)
{
    writer->writeStartElement(QStringLiteral("ImportGroup"));
    if (!importGroup->condition().isEmpty())
        writer->writeAttribute(QStringLiteral("Condition"), importGroup->condition());
    if (!importGroup->label().isEmpty())
        writer->writeAttribute(QStringLiteral("Label"), importGroup->label());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildImportGroup *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildItem *item)
{
    writer->writeStartElement(item->name());
    if (!item->include().isEmpty())
        writer->writeAttribute(QStringLiteral("Include"), item->include());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildItem *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildItemDefinitionGroup *itemDefinitionGroup)
{
    writer->writeStartElement(QStringLiteral("ItemDefinitionGroup"));
    if (!itemDefinitionGroup->condition().isEmpty())
        writer->writeAttribute(QStringLiteral("Condition"), itemDefinitionGroup->condition());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildItemDefinitionGroup *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildItemGroup *itemGroup)
{
    writer->writeStartElement(QStringLiteral("ItemGroup"));
    if (!itemGroup->condition().isEmpty())
        writer->writeAttribute(QStringLiteral("Condition"), itemGroup->condition());
    if (!itemGroup->label().isEmpty())
        writer->writeAttribute(QStringLiteral("Label"), itemGroup->label());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildItemGroup *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildItemMetadata *itemMetadata)
{
    QString stringValue;
    if (itemMetadata->value().type() == QVariant::Bool) {
        stringValue = itemMetadata->value().toBool()
                ? QStringLiteral("True")
                : QStringLiteral("False");
    } else {
        stringValue = itemMetadata->value().toString();
    }
    writer->writeTextElement(itemMetadata->name(), stringValue);
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildItemMetadata *)
{
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildProject *project)
{
    writer->writeStartElement(QStringLiteral("Project"));
    if (!project->defaultTargets().isEmpty())
        writer->writeAttribute(QStringLiteral("DefaultTargets"), project->defaultTargets());
    if (!project->toolsVersion().isEmpty())
        writer->writeAttribute(QStringLiteral("ToolsVersion"), project->toolsVersion());
    writer->writeAttribute(QStringLiteral("xmlns"), kMSBuildSchemaURI);
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildProject *)
{
    writer->writeEndElement();
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildProperty *property)
{
    QString stringValue;
    if (property->value().type() == QVariant::Bool)
        stringValue = property->value().toBool() ? QStringLiteral("True") : QStringLiteral("False");
    else
        stringValue = property->value().toString();
    writer->writeTextElement(property->name(), stringValue);
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildProperty *)
{
}

void MSBuildProjectWriterPrivate::visitStart(const MSBuildPropertyGroup *propertyGroup)
{
    writer->writeStartElement(QStringLiteral("PropertyGroup"));
    if (!propertyGroup->condition().isEmpty())
        writer->writeAttribute(QStringLiteral("Condition"), propertyGroup->condition());
    if (!propertyGroup->label().isEmpty())
        writer->writeAttribute(QStringLiteral("Label"), propertyGroup->label());
}

void MSBuildProjectWriterPrivate::visitEnd(const MSBuildPropertyGroup *)
{
    writer->writeEndElement();
}

} // namespace qbs
