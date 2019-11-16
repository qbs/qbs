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

#ifndef QBS_IAREWMSP430BUILDCONFIGURATIONGROUP_V7_H
#define QBS_IAREWMSP430BUILDCONFIGURATIONGROUP_V7_H

#include <generators/generatorutils.h>
#include <generators/xmlpropertygroup.h>

namespace qbs {
namespace iarew {
namespace msp430 {
namespace v7 {

class Msp430BuildConfigurationGroup final
        : public gen::xml::PropertyGroup
{
private:
    explicit Msp430BuildConfigurationGroup(
            const Project &qbsProject,
            const ProductData &qbsProduct,
            const std::vector<ProductData> &qbsProductDeps);

    friend class Msp430BuildConfigurationGroupFactory;
};

class Msp430BuildConfigurationGroupFactory final
        : public gen::xml::PropertyGroupFactory
{
public:
    bool canCreate(gen::utils::Architecture arch,
                   const Version &version) const final;

    std::unique_ptr<gen::xml::PropertyGroup> create(
            const Project &qbsProject,
            const ProductData &qbsProduct,
            const std::vector<ProductData> &qbsProductDeps) const final;
};

} // namespace v7
} // namespace msp430
} // namespace iarew
} // namespace qbs

#endif // QBS_IAREWMSP430BUILDCONFIGURATIONGROUP_V7_H
