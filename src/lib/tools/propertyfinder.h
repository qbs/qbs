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
#ifndef QBS_PROPERTY_FINDER_H
#define QBS_PROPERTY_FINDER_H

#include <QVariantList>
#include <QVariantMap>

namespace qbs {
namespace Internal {

class PropertyFinder
{
public:
    enum MergeType { DoMergeLists, DoNotMergeLists };
    QVariantList propertyValues(const QVariantMap &properties, const QString &moduleName,
                              const QString &key, MergeType mergeType = DoMergeLists);

    // Note that this can still be a list if the property type itself is one.
    QVariant propertyValue(const QVariantMap &properties, const QString &moduleName,
                         const QString &key);

private:
    void findModuleValues(const QVariantMap &properties);
    void addToList(const QVariant &value);
    static void mergeLists(QVariantList *values);

    QString m_moduleName;
    QString m_key;
    QVariantList m_values;
    bool m_findOnlyOne;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
