/****************************************************************************
**
** Copyright (C) 2022 Raphaël Cotty <raphael.cotty@gmail.com>
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

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

namespace qbs {
namespace Internal {

class Host : public QObject, QScriptable
{
    Q_OBJECT
public:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_architecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_os(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_platform(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osVersion(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osBuildVersion(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osVersionParts(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osVersionMajor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osVersionMinor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_osVersionPatch(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_nullDevice(QScriptContext *context, QScriptEngine *engine);
};

static QStringList osList()
{
    QStringList list;
    for (const auto &s : HostOsInfo::canonicalOSIdentifiers(HostOsInfo::hostOSIdentifier()))
        list.push_back(s);
    return list;
}

QScriptValue Host::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    return context->throwError(Tr::tr("'Host' cannot be instantiated."));
}

QScriptValue Host::js_architecture(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);

    return HostOsInfo::hostOSArchitecture();
}

QScriptValue Host::js_os(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    static QStringList host = osList();
    return engine->toScriptValue(host);
}

QScriptValue Host::js_platform(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    return HostOsInfo::hostOSIdentifier();
}

QScriptValue Host::js_osVersion(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static QString osVersion = HostOsInfo::hostOsVersion().toString();
    return osVersion.isNull() ? engine->undefinedValue() : osVersion;
}

QScriptValue Host::js_osBuildVersion(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static QString osBuildVersion = HostOsInfo::hostOsBuildVersion();
    return osBuildVersion.isNull() ? engine->undefinedValue() : osBuildVersion;
}

QScriptValue Host::js_osVersionParts(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static QStringList osVersionParts = HostOsInfo::hostOsVersion().toString().split(
                QStringLiteral("."));
    return qScriptValueFromSequence(engine, osVersionParts);
}

QScriptValue Host::js_osVersionMajor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static int osVersionMajor = HostOsInfo::hostOsVersion().majorVersion();
    return osVersionMajor;
}

QScriptValue Host::js_osVersionMinor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static int osVersionMinor = HostOsInfo::hostOsVersion().minorVersion();
    return osVersionMinor;
}

QScriptValue Host::js_osVersionPatch(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static int osVersionPatch = HostOsInfo::hostOsVersion().patchLevel();
    return osVersionPatch;
}

QScriptValue Host::js_nullDevice(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    Q_UNUSED(engine);
    static QString nullDevice = HostOsInfo::isWindowsHost() ? QStringLiteral("NUL") :
                                                              QStringLiteral("/dev/null");
    return nullDevice;
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionHost(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue hostObj = engine->newQMetaObject(&Host::staticMetaObject,
                                                  engine->newFunction(&Host::js_ctor));
    hostObj.setProperty(QStringLiteral("architecture"), engine->newFunction(Host::js_architecture));
    hostObj.setProperty(QStringLiteral("os"), engine->newFunction(Host::js_os));
    hostObj.setProperty(QStringLiteral("platform"), engine->newFunction(Host::js_platform));
    hostObj.setProperty(QStringLiteral("osVersion"), engine->newFunction(Host::js_osVersion));
    hostObj.setProperty(QStringLiteral("osBuildVersion"), engine->newFunction(
                            Host::js_osBuildVersion));
    hostObj.setProperty(QStringLiteral("osVersionParts"), engine->newFunction(
                            Host::js_osVersionParts));
    hostObj.setProperty(QStringLiteral("osVersionMajor"), engine->newFunction(
                            Host::js_osVersionMajor));
    hostObj.setProperty(QStringLiteral("osVersionMinor"), engine->newFunction(
                            Host::js_osVersionMinor));
    hostObj.setProperty(QStringLiteral("osVersionPatch"), engine->newFunction(
                            Host::js_osVersionPatch));
    hostObj.setProperty(QStringLiteral("nullDevice"), engine->newFunction(Host::js_nullDevice));
    extensionObject.setProperty(QStringLiteral("Host"), hostObj);
}

Q_DECLARE_METATYPE(qbs::Internal::Host *)

#include "host.moc"
