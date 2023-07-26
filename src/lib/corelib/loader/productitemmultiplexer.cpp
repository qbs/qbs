/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "productitemmultiplexer.h"

#include "loaderutils.h"
#include "moduleinstantiator.h"

#include <language/evaluator.h>
#include <language/item.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/translator.h>
#include <tools/scripttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <QJsonDocument>

#include <vector>

// This module deals with product multiplexing over the various defined axes.
// For instance, a product with qbs.architectures: ["x86", "arm"] will get multiplexed into
// two products with qbs.architecture: "x86" and qbs.architecture: "arm", respectively.
namespace qbs::Internal {
namespace {
using MultiplexRow = std::vector<VariantValuePtr>;
using MultiplexTable = std::vector<MultiplexRow>;
class MultiplexInfo
{
public:
    std::vector<QString> properties;
    MultiplexTable table;
    bool aggregate = false;
    VariantValuePtr multiplexedType;

    QString toIdString(size_t row, LoaderState &loaderState) const;
};
} // namespace

class ProductItemMultiplexer
{
public:
    ProductItemMultiplexer(const QString &productName, Item *productItem, Item *tempQbsModuleItem,
                           const std::function<void()> &dropTempQbsModule, LoaderState &loaderState)
        : m_loaderState(loaderState), m_productName(productName), m_productItem(productItem),
          m_tempQbsModuleItem(tempQbsModuleItem), m_dropTempQbsModule(dropTempQbsModule) {}

    QList<Item *> multiplex();
private:
    MultiplexInfo extractMultiplexInfo();
    MultiplexTable combine(const MultiplexTable &table, const MultiplexRow &values);

    LoaderState &m_loaderState;
    const QString &m_productName;
    Item * const m_productItem;
    Item * const m_tempQbsModuleItem;
    const std::function<void()> &m_dropTempQbsModule;
};

QList<Item *> ProductItemMultiplexer::multiplex()
{
    const auto multiplexInfo = extractMultiplexInfo();
    m_dropTempQbsModule();
    if (multiplexInfo.table.size() > 1)
        m_productItem->setProperty(StringConstants::multiplexedProperty(), VariantValue::trueValue());
    VariantValuePtr productNameValue = VariantValue::create(m_productName);
    Item *aggregator = multiplexInfo.aggregate ? m_productItem->clone(m_loaderState.itemPool())
                                               : nullptr;
    QList<Item *> additionalProductItems;
    std::vector<VariantValuePtr> multiplexConfigurationIdValues;
    for (size_t row = 0; row < multiplexInfo.table.size(); ++row) {
        Item *item = m_productItem;
        const auto &mprow = multiplexInfo.table.at(row);
        QBS_CHECK(mprow.size() == multiplexInfo.properties.size());
        if (row > 0) {
            item = m_productItem->clone(m_loaderState.itemPool());
            additionalProductItems.push_back(item);
        }
        const QString multiplexConfigurationId = multiplexInfo.toIdString(row, m_loaderState);
        const VariantValuePtr multiplexConfigurationIdValue
            = VariantValue::create(multiplexConfigurationId);
        if (multiplexInfo.table.size() > 1 || aggregator) {
            multiplexConfigurationIdValues.push_back(multiplexConfigurationIdValue);
            item->setProperty(StringConstants::multiplexConfigurationIdProperty(),
                              multiplexConfigurationIdValue);
        }
        if (multiplexInfo.multiplexedType)
            item->setProperty(StringConstants::typeProperty(), multiplexInfo.multiplexedType);
        for (size_t column = 0; column < mprow.size(); ++column) {
            Item * const qbsItem = retrieveQbsItem(item, m_loaderState);
            const QString &propertyName = multiplexInfo.properties.at(column);
            const VariantValuePtr &mpvalue = mprow.at(column);
            qbsItem->setProperty(propertyName, mpvalue);
        }
    }

    if (aggregator) {
        additionalProductItems << aggregator;

        // Add dependencies to all multiplexed instances.
        for (const auto &v : multiplexConfigurationIdValues) {
            Item *dependsItem = Item::create(&m_loaderState.itemPool(), ItemType::Depends);
            dependsItem->setProperty(StringConstants::nameProperty(), productNameValue);
            dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(), v);
            dependsItem->setProperty(StringConstants::profilesProperty(),
                                     VariantValue::create(QStringList()));
            dependsItem->setFile(aggregator->file());
            dependsItem->setupForBuiltinType(m_loaderState.parameters().deprecationWarningMode(),
                                             m_loaderState.logger());
            Item::addChild(aggregator, dependsItem);
        }
    }

    return additionalProductItems;
}

MultiplexInfo ProductItemMultiplexer::extractMultiplexInfo()
{
    static const QString mpmKey = QStringLiteral("multiplexMap");

    Evaluator &evaluator = m_loaderState.evaluator();
    JSContext * const ctx = evaluator.engine()->context();
    const ScopedJsValue multiplexMap(ctx, evaluator.value(m_tempQbsModuleItem, mpmKey));
    const QStringList multiplexByQbsProperties = evaluator.stringListValue(
        m_productItem, StringConstants::multiplexByQbsPropertiesProperty());

    MultiplexInfo multiplexInfo;
    multiplexInfo.aggregate = evaluator.boolValue(
        m_productItem, StringConstants::aggregateProperty());

    const QString multiplexedType = evaluator.stringValue(
        m_productItem, StringConstants::multiplexedTypeProperty());
    if (!multiplexedType.isEmpty())
        multiplexInfo.multiplexedType = VariantValue::create(multiplexedType);

    Set<QString> uniqueMultiplexByQbsProperties;
    for (const QString &key : multiplexByQbsProperties) {
        const QString mappedKey = getJsStringProperty(ctx, multiplexMap, key);
        if (mappedKey.isEmpty())
            throw ErrorInfo(Tr::tr("There is no entry for '%1' in 'qbs.multiplexMap'.").arg(key));

        if (!uniqueMultiplexByQbsProperties.insert(mappedKey).second)
            continue;

        const ScopedJsValue arr(ctx, evaluator.value(m_tempQbsModuleItem, key));
        if (JS_IsUndefined(arr))
            continue;
        if (!JS_IsArray(ctx, arr))
            throw ErrorInfo(Tr::tr("Property '%1' must be an array.").arg(key));

        const quint32 arrlen = getJsIntProperty(ctx, arr, StringConstants::lengthProperty());
        if (arrlen == 0)
            continue;

        MultiplexRow mprow;
        mprow.reserve(arrlen);
        QVariantList entriesForKey;
        for (quint32 i = 0; i < arrlen; ++i) {
            const ScopedJsValue sv(ctx, JS_GetPropertyUint32(ctx, arr, i));
            const QVariant value = getJsVariant(ctx, sv);
            if (entriesForKey.contains(value))
                continue;
            entriesForKey << value;
            mprow.push_back(VariantValue::create(value));
        }
        multiplexInfo.table = combine(multiplexInfo.table, mprow);
        multiplexInfo.properties.push_back(mappedKey);
    }
    return multiplexInfo;
}

MultiplexTable ProductItemMultiplexer::combine(const MultiplexTable &table,
                                               const MultiplexRow &values)
{
    MultiplexTable result;
    if (table.empty()) {
        result.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            MultiplexRow row;
            row.resize(1);
            row[0] = values.at(i);
            result[i] = row;
        }
    } else {
        for (const auto &row : table) {
            for (const auto &value : values) {
                MultiplexRow newRow = row;
                newRow.push_back(value);
                result.push_back(newRow);
            }
        }
    }
    return result;
}

QString MultiplexInfo::toIdString(size_t row, LoaderState &loaderState) const
{
    const auto &mprow = table.at(row);
    QVariantMap multiplexConfiguration;
    for (size_t column = 0; column < mprow.size(); ++column) {
        const QString &propertyName = properties.at(column);
        const VariantValuePtr &mpvalue = mprow.at(column);
        multiplexConfiguration.insert(propertyName, mpvalue->value());
    }
    QString id = QString::fromUtf8(QJsonDocument::fromVariant(multiplexConfiguration)
                                   .toJson(QJsonDocument::Compact)
                                   .toBase64());

    // Cache for later use in multiplexIdToVariantMap()
    loaderState.topLevelProject().addMultiplexConfiguration(id, multiplexConfiguration);

    return id;
}

QList<Item *> multiplex(const QString &productName, Item *productItem, Item *tempQbsModuleItem,
                        const std::function<void ()> &dropTempQbsModule, LoaderState &loaderState)
{
    return ProductItemMultiplexer(productName, productItem, tempQbsModuleItem, dropTempQbsModule,
                                  loaderState).multiplex();
}

} // namespace qbs::Internal
