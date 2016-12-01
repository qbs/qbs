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

#include "visualstudioguidpool.h"
#include <tools/filesaver.h>
#include <QtCore/qfile.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

namespace qbs {

class VisualStudioGuidPoolPrivate
{
public:
    QString storeFilePath;
    QMap<QString, QUuid> productGuids;
};

VisualStudioGuidPool::VisualStudioGuidPool(const QString &storeFilePath)
    : d(new VisualStudioGuidPoolPrivate)
{
    // Read any existing GUIDs from the on-disk store
    QFile file(d->storeFilePath = storeFilePath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const auto data = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();

        QMapIterator<QString, QVariant> it = data;
        while (it.hasNext()) {
            it.next();
            d->productGuids.insert(it.key(), QUuid(it.value().toString()));
        }
    }
}

VisualStudioGuidPool::~VisualStudioGuidPool()
{
    Internal::FileSaver file(d->storeFilePath);
    if (file.open()) {
        QVariantMap productData;
        QMapIterator<QString, QUuid> it(d->productGuids);
        while (it.hasNext()) {
            it.next();
            productData.insert(it.key(), it.value().toString());
        }

        file.write(QJsonDocument::fromVariant(productData).toJson());
        file.commit();
    }
}

QUuid VisualStudioGuidPool::drawProductGuid(const QString &productName)
{
    if (!d->productGuids.contains(productName))
        d->productGuids.insert(productName, QUuid::createUuid());
    return d->productGuids.value(productName);
}

} // namespace qbs
