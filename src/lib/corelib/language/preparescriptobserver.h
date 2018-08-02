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
#ifndef QBS_PREPARESCRIPTOBSERVER_H
#define QBS_PREPARESCRIPTOBSERVER_H

#include "qualifiedid.h"
#include "scriptpropertyobserver.h"

#include <tools/set.h>

#include <QtCore/qstring.h>

#include <unordered_map>

namespace qbs {
namespace Internal {
class ResolvedProduct;
class ScriptEngine;

class PrepareScriptObserver : public ScriptPropertyObserver
{
public:
    PrepareScriptObserver(ScriptEngine *engine, UnobserveMode unobserveMode);

    void addProjectObjectId(qint64 projectId, const QString &projectName)
    {
        m_projectObjectIds.insert(std::make_pair(projectId, projectName));
    }

    void addExportsObjectId(qint64 exportsId, const ResolvedProduct *product)
    {
        m_exportsObjectIds.insert(std::make_pair(exportsId, product));
    }

    void addArtifactId(qint64 artifactId) { m_artifactIds.insert(artifactId); }
    bool addImportId(qint64 importId) { return m_importIds.insert(importId).second; }
    void clearImportIds() { m_importIds.clear(); }
    void addParameterObjectId(qint64 id, const QString &productName, const QString &depName,
                              const QualifiedId &moduleName)
    {
        const QString depAndModuleName = depName + QLatin1Char(':') + moduleName.toString();
        const auto value = std::make_pair(productName, depAndModuleName);
        m_parameterObjects.insert(std::make_pair(id, value));
    }

private:
    void onPropertyRead(const QScriptValue &object, const QString &name,
                        const QScriptValue &value) override;

    std::unordered_map<qint64, QString> m_projectObjectIds;
    std::unordered_map<qint64, std::pair<QString, QString>> m_parameterObjects;
    std::unordered_map<qint64, const ResolvedProduct *> m_exportsObjectIds;
    Set<qint64> m_importIds;
    Set<qint64> m_artifactIds;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
