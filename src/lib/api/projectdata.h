/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#ifndef PUBLICTYPES_H
#define PUBLICTYPES_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qbs {
namespace Internal { class ProjectPrivate; }

// TODO: explicitly shared?

class GroupData
{
    friend class Internal::ProjectPrivate;
public:
    GroupData();

    int qbsLine() const { return m_qbsLine; }
    QString name() const { return m_name; }
    QStringList filePaths() const { return m_filePaths; }
    QStringList expandedWildcards() const { return m_expandedWildcards; }
    QVariantMap properties() const { return m_properties; }

    // TODO: Filter out double entries here or somewhere else?
    QStringList allFilePaths() const { return filePaths() + expandedWildcards(); }

private:
    QString m_name;
    int m_qbsLine;
    QStringList m_filePaths;
    QStringList m_expandedWildcards;
    QVariantMap m_properties;
};

bool operator==(const GroupData &lhs, const GroupData &rhs);
bool operator!=(const GroupData &lhs, const GroupData &rhs);

class ProductData
{
    friend class Internal::ProjectPrivate;
public:
    ProductData();

    QString name() const { return m_name; }
    QString qbsFilePath() const { return m_qbsFilePath; }
    int qbsLine() const { return m_qbsLine; }
    QStringList fileTags() const { return m_fileTags; }
    QVariantMap properties() const { return m_properties; }
    QList<GroupData> groups() const { return m_groups; }

private:
    QString m_name;
    QString m_qbsFilePath;
    int m_qbsLine;
    QStringList m_fileTags;
    QVariantMap m_properties;
    QList<GroupData> m_groups;
};

bool operator==(const ProductData &lhs, const ProductData &rhs);
bool operator!=(const ProductData &lhs, const ProductData &rhs);

class ProjectData
{
    friend class Internal::ProjectPrivate;
public:
    ProjectData();

    QString qbsFilePath() const { return m_qbsFilePath; }
    QList<ProductData> products() const { return m_products; }

private:
    QString m_qbsFilePath;
    QList<ProductData> m_products;
};

bool operator==(const ProjectData &lhs, const ProjectData &rhs);
bool operator!=(const ProjectData &lhs, const ProjectData &rhs);

} // namespace qbs

#endif // PUBLICTYPES_H
