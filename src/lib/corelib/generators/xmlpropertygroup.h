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

#ifndef GENERATORS_XML_PROPERTY_GROUP_H
#define GENERATORS_XML_PROPERTY_GROUP_H

#include "generatorversioninfo.h"
#include "xmlproperty.h"

#include <tools/qbs_export.h>

#include <memory>

namespace qbs {

class ProductData;
class Project;

namespace gen {
namespace xml {

class QBS_EXPORT PropertyGroup : public Property
{
public:
    explicit PropertyGroup(QByteArray name);

    void appendProperty(QByteArray name, QVariant value);
    void appendMultiLineProperty(QByteArray key, const QStringList &values,
                                 QChar sep = QLatin1Char(','));

    void accept(INodeVisitor *visitor) const final;
};

class PropertyGroupFactory
{
public:
    virtual ~PropertyGroupFactory() = default;
    virtual bool canCreate(utils::Architecture arch,
                           const Version &version) const = 0;

    virtual std::unique_ptr<PropertyGroup> create(
            const qbs::Project &qbsProject,
            const qbs::ProductData &qbsProduct,
            const std::vector<ProductData> &qbsProductDeps) const = 0;
};

} // namespace xml
} // namespace gen
} // namespace qbs

#endif // GENERATORS_XML_PROPERTY_GROUP_H
