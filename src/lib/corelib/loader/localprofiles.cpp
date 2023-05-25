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

#include "localprofiles.h"

#include "loaderutils.h"

#include <language/evaluator.h>
#include <language/item.h>
#include <language/qualifiedid.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/translator.h>
#include <tools/profile.h>
#include <tools/scripttools.h>
#include <tools/stringconstants.h>

namespace qbs::Internal {
class LocalProfiles::Private
{
public:
    Private(LoaderState &loaderState) : loaderState(loaderState) {}

    void handleProfile(Item *profileItem);
    void evaluateProfileValues(const QualifiedId &namePrefix, Item *item, Item *profileItem,
                               QVariantMap &values);
    void collectProfiles(Item *productOrProject, Item *projectScope);

    LoaderState &loaderState;
    QVariantMap profiles;
};

LocalProfiles::LocalProfiles(LoaderState &loaderState) : d(makePimpl<Private>(loaderState)) {}
LocalProfiles::~LocalProfiles() = default;

void LocalProfiles::collectProfilesFromItems(Item *productOrProject, Item *projectScope)
{
    d->collectProfiles(productOrProject, projectScope);
}

const QVariantMap &LocalProfiles::profiles() const
{
    return d->profiles;
}

void LocalProfiles::Private::handleProfile(Item *profileItem)
{
    QVariantMap values;
    evaluateProfileValues(QualifiedId(), profileItem, profileItem, values);
    const bool condition = values.take(StringConstants::conditionProperty()).toBool();
    if (!condition)
        return;
    const QString profileName = values.take(StringConstants::nameProperty()).toString();
    if (profileName.isEmpty())
        throw ErrorInfo(Tr::tr("Every Profile item must have a name"), profileItem->location());
    if (profileName == Profile::fallbackName()) {
        throw ErrorInfo(Tr::tr("Reserved name '%1' cannot be used for an actual profile.")
                            .arg(profileName), profileItem->location());
    }
    if (profiles.contains(profileName)) {
        throw ErrorInfo(Tr::tr("Local profile '%1' redefined.").arg(profileName),
                        profileItem->location());
    }
    profiles.insert(profileName, values);
}

void LocalProfiles::Private::evaluateProfileValues(const QualifiedId &namePrefix, Item *item,
                                                   Item *profileItem, QVariantMap &values)
{
    const Item::PropertyMap &props = item->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QualifiedId name = namePrefix;
        name << it.key();
        switch (it.value()->type()) {
        case Value::ItemValueType:
            evaluateProfileValues(name, std::static_pointer_cast<ItemValue>(it.value())->item(),
                                  profileItem, values);
            break;
        case Value::VariantValueType:
            values.insert(name.join(QLatin1Char('.')),
                          std::static_pointer_cast<VariantValue>(it.value())->value());
            break;
        case Value::JSSourceValueType:
            if (item != profileItem)
                item->setScope(profileItem);
            const ScopedJsValue sv(loaderState.evaluator().engine()->context(),
                                   loaderState.evaluator().value(item, it.key()));
            values.insert(name.join(QLatin1Char('.')),
                          getJsVariant(loaderState.evaluator().engine()->context(), sv));
            break;
        }
    }
}

void LocalProfiles::Private::collectProfiles(Item *productOrProject, Item *projectScope)
{
    Item * scope = productOrProject->type() == ItemType::Project ? projectScope : nullptr;
    for (auto it = productOrProject->children().begin();
         it != productOrProject->children().end();) {
        Item * const childItem = *it;
        if (childItem->type() == ItemType::Profile) {
            if (!scope) {
                const ItemValuePtr itemValue = ItemValue::create(productOrProject);
                scope = Item::create(productOrProject->pool(), ItemType::Scope);
                scope->setProperty(StringConstants::productVar(), itemValue);
                scope->setFile(productOrProject->file());
                scope->setScope(projectScope);
            }
            childItem->setScope(scope);
            try {
                handleProfile(childItem);
            } catch (const ErrorInfo &e) {
                handlePropertyError(e, loaderState.parameters(), loaderState.logger());
            }
            it = productOrProject->children().erase(it); // TODO: delete item and scope
        } else {
            if (childItem->type() == ItemType::Product)
                collectProfiles(childItem, projectScope);
            ++it;
        }
    }
}

} // namespace qbs::Internal
