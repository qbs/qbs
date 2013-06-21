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
#ifndef QBS_PROJECTDATA_H
#define QBS_PROJECTDATA_H

#include "../tools/codelocation.h"
#include "../tools/qbs_export.h"

#include <QExplicitlySharedDataPointer>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qbs {
namespace Internal {
class GroupDataPrivate;
class ProductDataPrivate;
class ProjectPrivate;
class ProjectDataPrivate;
class PropertyMapPrivate;
} // namespace Internal

class PropertyMap;

bool operator==(const PropertyMap &pm1, const PropertyMap &pm2);
bool operator!=(const PropertyMap &pm1, const PropertyMap &pm2);

class QBS_EXPORT PropertyMap
{
    friend class Internal::ProjectPrivate;
    friend bool operator==(const PropertyMap &, const PropertyMap &);
    friend bool operator!=(const PropertyMap &, const PropertyMap &);

public:
    PropertyMap();
    PropertyMap(const PropertyMap &other);
    ~PropertyMap();

    PropertyMap &operator =(const PropertyMap &other);

    QStringList allProperties() const;
    QVariant getProperty(const QString &name) const;

    QVariantList getModuleProperties(const QString &moduleName, const QString &propertyName) const;
    QStringList getModulePropertiesAsStringList(const QString &moduleName,
                                                 const QString &propertyName) const;
    QVariant getModuleProperty(const QString &moduleName, const QString &propertyName) const;

    // For debugging.
    QString toString() const;

private:
    Internal::PropertyMapPrivate *d;
};

class QBS_EXPORT GroupData
{
    friend class Internal::ProjectPrivate;
public:
    GroupData();
    GroupData(const GroupData &other);
    GroupData &operator=(const GroupData &other);
    ~GroupData();

    bool isValid() const;

    CodeLocation location() const;
    QString name() const;
    QStringList filePaths() const;
    QStringList expandedWildcards() const;
    PropertyMap properties() const;
    bool isEnabled() const;
    QStringList allFilePaths() const;

private:
    QExplicitlySharedDataPointer<Internal::GroupDataPrivate> d;
};

QBS_EXPORT bool operator==(const GroupData &lhs, const GroupData &rhs);
QBS_EXPORT bool operator!=(const GroupData &lhs, const GroupData &rhs);
QBS_EXPORT bool operator<(const GroupData &lhs, const GroupData &rhs);

class QBS_EXPORT ProductData
{
    friend class Internal::ProjectPrivate;
public:
    ProductData();
    ProductData(const ProductData &other);
    ProductData &operator=(const ProductData &other);
    ~ProductData();

    bool isValid() const;

    QString name() const;
    CodeLocation location() const;
    QStringList fileTags() const;
    PropertyMap properties() const;
    QList<GroupData> groups() const;
    bool isEnabled() const;

private:
    QExplicitlySharedDataPointer<Internal::ProductDataPrivate> d;
};

QBS_EXPORT bool operator==(const ProductData &lhs, const ProductData &rhs);
QBS_EXPORT bool operator!=(const ProductData &lhs, const ProductData &rhs);
QBS_EXPORT bool operator<(const ProductData &lhs, const ProductData &rhs);

class QBS_EXPORT ProjectData
{
    friend class Internal::ProjectPrivate;
public:
    ProjectData();
    ProjectData(const ProjectData &other);
    ProjectData &operator=(const ProjectData &other);
    ~ProjectData();

    bool isValid() const;

    QString name() const;
    CodeLocation location() const;
    bool isEnabled() const;
    QString buildDirectory() const;
    QList<ProductData> products() const;
    QList<ProjectData> subProjects() const;
    QList<ProductData> allProducts() const;

private:
    QExplicitlySharedDataPointer<Internal::ProjectDataPrivate> d;
};

QBS_EXPORT bool operator==(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator!=(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator<(const ProjectData &lhs, const ProjectData &rhs);

} // namespace qbs

#endif // QBS_PROJECTDATA_H
