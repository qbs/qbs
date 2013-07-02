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

#include "jsextensions.h"

#include "domxml.h"
#include "file.h"
#include "process.h"
#include "textfile.h"

namespace qbs {
namespace Internal {

void JsExtensions::setupExtensions(const QStringList &names, QScriptValue scope)
{
    foreach (const QString &name, names)
        initializers().value(name)(scope);
}

bool JsExtensions::hasExtension(const QString &name)
{
    return initializers().contains(name);
}

JsExtensions::InitializerMap JsExtensions::initializers()
{
    if (m_initializers.isEmpty()) {
        m_initializers.insert(QLatin1String("File"), &initializeJsExtensionFile);
        m_initializers.insert(QLatin1String("Process"), &initializeJsExtensionProcess);
        m_initializers.insert(QLatin1String("Xml"), &initializeJsExtensionXml);
        m_initializers.insert(QLatin1String("TextFile"), &initializeJsExtensionTextFile);
    }
    return m_initializers;
}

JsExtensions::InitializerMap JsExtensions::m_initializers;

} // namespace Internal
} // namespace qbs
