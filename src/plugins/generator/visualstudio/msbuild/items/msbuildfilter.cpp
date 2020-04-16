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

#include "msbuildfilter.h"
#include "../msbuilditemmetadata.h"
#include <tools/hostosinfo.h>
#include <QtCore/quuid.h>

namespace qbs {

static const QString MSBuildFilterItemName = QStringLiteral("Filter");

class MSBuildFilterPrivate
{
public:
    QUuid identifier;
    QList<QString> extensions;
    bool parseFiles = true;
    bool sourceControlFiles = true;
    MSBuildItemMetadata *identifierMetadata = nullptr;
    MSBuildItemMetadata *extensionsMetadata = nullptr;
};

MSBuildFilter::MSBuildFilter(IMSBuildItemGroup *parent)
    : MSBuildItem(MSBuildFilterItemName, parent)
    , d(new MSBuildFilterPrivate)
{
    d->identifierMetadata = new MSBuildItemMetadata(QStringLiteral("UniqueIdentifier"),
                                                    QVariant(), this);
    d->extensionsMetadata = new MSBuildItemMetadata(QStringLiteral("Extensions"),
                                                    QVariant(), this);
    setIdentifier(QUuid::createUuid());
}

MSBuildFilter::MSBuildFilter(const QString &name,
                             const QList<QString> &extensions,
                             IMSBuildItemGroup *parent)
    : MSBuildFilter(parent)
{
    setInclude(name);
    setExtensions(extensions);
}

MSBuildFilter::~MSBuildFilter() = default;

QUuid MSBuildFilter::identifier() const
{
    return d->identifier;
}

void MSBuildFilter::setIdentifier(const QUuid &identifier)
{
    d->identifier = identifier;
    d->identifierMetadata->setValue(identifier.toString());
}

QList<QString> MSBuildFilter::extensions() const
{
    return d->extensions;
}

void MSBuildFilter::setExtensions(const QList<QString> &extensions)
{
    d->extensions = extensions;
    d->extensionsMetadata->setValue(QStringList(extensions).join(
                                        Internal::HostOsInfo::pathListSeparator(
                                            Internal::HostOsInfo::HostOsWindows)));
}

bool MSBuildFilter::parseFiles() const
{
    return d->parseFiles;
}

void MSBuildFilter::setParseFiles(bool parseFiles)
{
    d->parseFiles = parseFiles;
}

bool MSBuildFilter::sourceControlFiles() const
{
    return d->sourceControlFiles;
}

void MSBuildFilter::setSourceControlFiles(bool sourceControlFiles)
{
    d->sourceControlFiles = sourceControlFiles;
}

} // namespace qbs
