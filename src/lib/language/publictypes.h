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

#include <QtGlobal>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qbs {

template<int dummy> class IdTemplate;
template<int n> uint qHash(IdTemplate<n> id);

// Instantiate this template with a unique parameter to get a new type-safe id class.
template<int dummy> class IdTemplate
{
    friend class QbsEngine;
    friend uint qHash<>(IdTemplate<dummy> id);
public:
    IdTemplate() : m_id(0), m_timeStamp(0) {}

    bool isValid() const { return m_id; }
    bool operator==(IdTemplate<dummy> other) const {
        return m_id == other.m_id && m_timeStamp == other.m_timeStamp;
    }

private:
    explicit IdTemplate(quintptr id) : m_id(id), m_timeStamp(QDateTime::currentMSecsSinceEpoch()) {}

    quintptr m_id;
    qint64 m_timeStamp;
};

template<int n> uint qHash(IdTemplate<n> id) {
    return QT_PREPEND_NAMESPACE(qHash)(qMakePair(id.m_id, id.m_timeStamp));
}


class Group
{
    friend class QbsEngine;
public:
    typedef IdTemplate<0> Id;

    Group();

    Id id() const { return m_id; }
    int qbsLine() const { return m_qbsLine; }
    QString name() const { return m_name; }
    QStringList filePaths() const { return m_filePaths; }
    QStringList expandedWildcards() const { return m_expandedWildcards; }
    QVariantMap properties() const { return m_properties; }

    // TODO: Filter out double entries here or somewhere else?
    QStringList allFilePaths() const { return filePaths() + expandedWildcards(); }

private:
    explicit Group(Id id) : m_id(id) {}

    Id m_id;
    QString m_name;
    int m_qbsLine;
    QStringList m_filePaths;
    QStringList m_expandedWildcards;
    QVariantMap m_properties;
};


class Product
{
    friend class QbsEngine;
public:
    typedef IdTemplate<1> Id;

    Product();

    Id id() const { return m_id; }
    QString name() const { return m_name; }
    QString qbsFilePath() const { return m_qbsFilePath; }
    int qbsLine() const { return m_qbsLine; }
    QStringList fileTags() const { return m_fileTags; }
    QVariantMap properties() const { return m_properties; }
    QList<Group> groups() const { return m_groups; }

private:
    explicit Product(Id id) : m_id(id) {}

    Id m_id;
    QString m_name;
    QString m_qbsFilePath;
    int m_qbsLine;
    QStringList m_fileTags;
    QVariantMap m_properties;
    QList<Group> m_groups;
};


class Project
{
    friend class QbsEngine;
public:
    typedef IdTemplate<2> Id;

    Project();

    Id id() const { return m_id; }
    QString qbsFilePath() const { return m_qbsFilePath; }
    QList<Product> products() const { return m_products; }

private:
    explicit Project(Id id) : m_id(id) {}

    Id m_id;
    QString m_qbsFilePath;
    QList<Product> m_products;
};

} // namespace qbs

#endif // PUBLICTYPES_H
