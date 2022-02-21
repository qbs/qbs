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

#include "jsextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

namespace qbs {
namespace Internal {

class Host : public JsExtension<Host>
{
public:
    static const char *name() { return "Host"; }
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static JSValue jsArchitecture(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return makeJsString(ctx, HostOsInfo::hostOSArchitecture());
    }
    static JSValue jsOs(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsPlatform(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return makeJsString(ctx, HostOsInfo::hostOSIdentifier());
    }
    static JSValue jsOsVersion(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsOsBuildVersion(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsOsVersionParts(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsOsVersionMajor(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsOsVersionMinor(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsOsVersionPatch(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsNullDevice(JSContext *ctx, JSValueConst, int, JSValueConst *);
};

static QStringList osList()
{
    QStringList list;
    for (const auto &s : HostOsInfo::canonicalOSIdentifiers(HostOsInfo::hostOSIdentifier()))
        list.push_back(s);
    return list;
}

void Host::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, "architecture", &Host::jsArchitecture, 0);
    setupMethod(ctx, classObj, "os", &Host::jsOs, 0);
    setupMethod(ctx, classObj, "platform", &Host::jsPlatform, 0);
    setupMethod(ctx, classObj, "osVersion", &Host::jsOsVersion, 0);
    setupMethod(ctx, classObj, "osBuildVersion", &Host::jsOsBuildVersion, 0);
    setupMethod(ctx, classObj, "osVersionParts", &Host::jsOsVersionParts, 0);
    setupMethod(ctx, classObj, "osVersionMajor", &Host::jsOsVersionMajor, 0);
    setupMethod(ctx, classObj, "osVersionMinor", &Host::jsOsVersionMinor, 0);
    setupMethod(ctx, classObj, "osVersionPatch", &Host::jsOsVersionPatch, 0);
    setupMethod(ctx, classObj, "nullDevice", &Host::jsNullDevice, 0);
}

JSValue Host::jsOs(JSContext *ctx, JSValue, int, JSValue *)
{
    static QStringList host = osList();
    return makeJsStringList(ctx, host);
}

JSValue Host::jsOsVersion(JSContext *ctx, JSValue, int, JSValue *)
{
    static QString osVersion = HostOsInfo::hostOsVersion().toString();
    return osVersion.isNull() ? JS_UNDEFINED : makeJsString(ctx, osVersion);
}

JSValue Host::jsOsBuildVersion(JSContext *ctx, JSValue, int, JSValue *)
{
    static QString osBuildVersion = HostOsInfo::hostOsBuildVersion();
    return osBuildVersion.isNull() ? JS_UNDEFINED : makeJsString(ctx, osBuildVersion);
}

JSValue Host::jsOsVersionParts(JSContext *ctx, JSValue, int, JSValue *)
{
    static QStringList osVersionParts = HostOsInfo::hostOsVersion().toString().split(
                QStringLiteral("."));
    return makeJsStringList(ctx, osVersionParts);
}

JSValue Host::jsOsVersionMajor(JSContext *ctx, JSValue, int, JSValue *)
{
    static int osVersionMajor = HostOsInfo::hostOsVersion().majorVersion();
    return JS_NewInt32(ctx, osVersionMajor);
}

JSValue Host::jsOsVersionMinor(JSContext *ctx, JSValue, int, JSValue *)
{
    static int osVersionMinor = HostOsInfo::hostOsVersion().minorVersion();
    return JS_NewInt32(ctx, osVersionMinor);
}

JSValue Host::jsOsVersionPatch(JSContext *ctx, JSValue, int, JSValue *)
{
    static int osVersionPatch = HostOsInfo::hostOsVersion().patchLevel();
    return JS_NewInt32(ctx, osVersionPatch);
}

JSValue Host::jsNullDevice(JSContext *ctx, JSValue, int, JSValue *)
{
    static QString nullDevice = HostOsInfo::isWindowsHost() ? QStringLiteral("NUL") :
                                                              QStringLiteral("/dev/null");
    return makeJsString(ctx, nullDevice);
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionHost(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::Host::registerClass(engine, extensionObject);
}
