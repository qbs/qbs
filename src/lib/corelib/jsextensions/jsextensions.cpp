/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "jsextensions.h"

#include "domxml.h"
#include "environmentextension.h"
#include "file.h"
#include "fileinfoextension.h"
#include "process.h"
#include "propertylist.h"
#include "temporarydir.h"
#include "textfile.h"
#include "utilitiesextension.h"

#include <QScriptEngine>

#include <utility>

namespace qbs {
namespace Internal {

typedef QHash<QString, void (*)(QScriptValue)> InitializerMap;
static InitializerMap initializers()
{
    static const InitializerMap theMap = {
        std::make_pair(QLatin1String("Environment"), &initializeJsExtensionEnvironment),
        std::make_pair(QLatin1String("File"), &initializeJsExtensionFile),
        std::make_pair(QLatin1String("FileInfo"), &initializeJsExtensionFileInfo),
        std::make_pair(QLatin1String("Process"), &initializeJsExtensionProcess),
        std::make_pair(QLatin1String("Xml"), &initializeJsExtensionXml),
        std::make_pair(QLatin1String("TemporaryDir"), &initializeJsExtensionTemporaryDir),
        std::make_pair(QLatin1String("TextFile"), &initializeJsExtensionTextFile),
        std::make_pair(QLatin1String("PropertyList"), &initializeJsExtensionPropertyList),
        std::make_pair(QLatin1String("Utilities"), &initializeJsExtensionUtilities)
    };
    return theMap;
}

void JsExtensions::setupExtensions(const QStringList &names, QScriptValue scope)
{
    foreach (const QString &name, names)
        initializers().value(name)(scope);
}

QScriptValue JsExtensions::loadExtension(QScriptEngine *engine, const QString &name)
{
    if (!hasExtension(name))
        return QScriptValue();

    QScriptValue extensionObj = engine->newObject();
    initializers().value(name)(extensionObj);
    return extensionObj.property(name);
}

bool JsExtensions::hasExtension(const QString &name)
{
    return initializers().contains(name);
}

} // namespace Internal
} // namespace qbs
