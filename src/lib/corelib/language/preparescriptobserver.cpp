/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "preparescriptobserver.h"

#include "property.h"
#include "scriptengine.h"

#include <buildgraph/artifact.h>
#include <language/language.h>
#include <tools/scripttools.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

PrepareScriptObserver::PrepareScriptObserver(ScriptEngine *engine, UnobserveMode unobserveMode)
    : ScriptPropertyObserver(engine, unobserveMode)
{
}

void PrepareScriptObserver::onPropertyRead(const QScriptValue &object, const QString &name,
                                           const QScriptValue &value)
{
    const auto objectId = object.objectId();
    const auto projectIt = m_projectObjectIds.find(objectId);
    if (projectIt != m_projectObjectIds.cend()) {
        engine()->addPropertyRequestedInScript(
                    Property(projectIt->second, QString(), name, value.toVariant(),
                             Property::PropertyInProject));
        return;
    }
    if (m_importIds.contains(objectId)) {
        engine()->addImportRequestedInScript(object.objectId());
        return;
    }
    const auto exportsIt = m_exportsObjectIds.find(value.objectId());
    if (exportsIt != m_exportsObjectIds.cend()) {
        engine()->addRequestedExport(exportsIt->second);
        return;
    }
    const auto it = m_parameterObjects.find(objectId);
    if (it != m_parameterObjects.cend()) {
        engine()->addPropertyRequestedInScript(
                    Property(it->second.first, it->second.second, name, value.toVariant(),
                             Property::PropertyInParameters));
    }
    if (name == StringConstants::fileTagsProperty() && m_artifactIds.contains(objectId)) {
        const Artifact * const artifact = attachedPointer<Artifact>(object);
        QBS_CHECK(artifact);
        const Property p(artifact->product->uniqueName(), QString(), name, value.toVariant(),
                         Property::PropertyInArtifact);
        engine()->addPropertyRequestedFromArtifact(artifact, p);
    }
}

} // namespace Internal
} // namespace qbs
