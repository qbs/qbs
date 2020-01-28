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

#include "iarewoptionpropertygroup.h"
#include "iarewsettingspropertygroup.h"

namespace qbs {

constexpr int kDataWantNonLocalPropertyValue = 1;

IarewSettingsPropertyGroup::IarewSettingsPropertyGroup()
    : gen::xml::PropertyGroup(QByteArrayLiteral("settings"))
{
    // Append name property item.
    m_nameProperty = appendChild<gen::xml::Property>(
                QByteArrayLiteral("name"), QVariant{});

    // Append archive version property item.
    m_archiveVersionProperty = appendChild<gen::xml::Property>(
                QByteArrayLiteral("archiveVersion"), QVariant{});

    // Append data property group item.
    m_dataPropertyGroup = appendChild<gen::xml::PropertyGroup>(
                QByteArrayLiteral("data"));
    // Append data version property item.
    m_dataVersionProperty = m_dataPropertyGroup->appendChild<
            gen::xml::Property>(
                QByteArrayLiteral("version"), QVariant{});
    // Append data want non-local property item.
    m_dataPropertyGroup->appendChild<gen::xml::Property>(
                QByteArrayLiteral("wantNonLocal"),
                kDataWantNonLocalPropertyValue);
    // Append data debug property item.
    m_dataDebugProperty = m_dataPropertyGroup->appendChild<
            gen::xml::Property>(
                QByteArrayLiteral("debug"), QVariant{});
}

void IarewSettingsPropertyGroup::setName(const QByteArray &name)
{
    // There is no way to move-construct a QVariant from T, thus name is shallow-copied
    m_nameProperty->setValue(QVariant(name));
}

QByteArray IarewSettingsPropertyGroup::name() const
{
    return m_nameProperty->value().toByteArray();
}

void IarewSettingsPropertyGroup::setArchiveVersion(
        int archiveVersion)
{
    m_archiveVersionProperty->setValue(archiveVersion);
}

int IarewSettingsPropertyGroup::archiveVersion() const
{
    return m_archiveVersionProperty->value().toInt();
}

void IarewSettingsPropertyGroup::setDataVersion(int dataVersion)
{
    m_dataVersionProperty->setValue(dataVersion);
}

void IarewSettingsPropertyGroup::setDataDebugInfo(int debugInfo)
{
    m_dataDebugProperty->setValue(debugInfo);
}

void IarewSettingsPropertyGroup::addOptionsGroup(
        const QByteArray &name,
        QVariantList states,
        int version)
{
    m_dataPropertyGroup->appendChild<IarewOptionPropertyGroup>(name, std::move(states), version);
}

} // namespace qbs
