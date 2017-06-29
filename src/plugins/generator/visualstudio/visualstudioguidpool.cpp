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

#include <map>

#include <json.h>

using namespace Json;

namespace qbs {

class VisualStudioGuidPoolPrivate
{
public:
    QString storeFilePath;
    std::map<std::string, QUuid> productGuids;
};

VisualStudioGuidPool::VisualStudioGuidPool(const QString &storeFilePath)
    : d(new VisualStudioGuidPoolPrivate)
{
    // Read any existing GUIDs from the on-disk store
    QFile file(d->storeFilePath = storeFilePath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const auto data = JsonDocument::fromJson(file.readAll().toStdString()).object();
        for (auto it = data.constBegin(), end = data.constEnd(); it != end; ++it) {
            d->productGuids.insert({
                it.key(),
                QUuid(QString::fromStdString(it.value().toString()))
            });
        }
    }
}

VisualStudioGuidPool::~VisualStudioGuidPool()
{
    Internal::FileSaver file(d->storeFilePath);
    if (file.open()) {
        JsonObject productData;
        for (const auto &it : d->productGuids)
            productData.insert(it.first, it.second.toString().toStdString());

        const auto data = JsonDocument(productData).toJson();
        file.write(QByteArray::fromRawData(data.data(), int(data.size())));
        file.commit();
    }
}

QUuid VisualStudioGuidPool::drawProductGuid(const QString &productName)
{
    if (d->productGuids.find(productName.toStdString()) == d->productGuids.cend())
        d->productGuids.insert({ productName.toStdString(), QUuid::createUuid() });
    return d->productGuids.at(productName.toStdString());
}

} // namespace qbs
