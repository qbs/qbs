/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
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

#include <QtCore/qfileinfo.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

static bool tempDirIsCanonical()
{
#if QT_VERSION >= 0x050c00
    return true;
#endif
    return false;
}

class TemporaryDir : public JsExtension<TemporaryDir>
{
    friend class JsExtension<TemporaryDir>;
public:
    static const char *name() { return "TemporaryDir"; }
    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int)
    {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{DubiousContext(EvalContext::PropertyEvaluation,
                                                                DubiousContext::SuggestMoving)};
        se->checkContext(QStringLiteral("qbs.TemporaryDir"), dubiousContexts);
        return createObject(ctx);
    }
    static void setupMethods(JSContext *ctx, JSValue obj)
    {
        setupMethod(ctx, obj, "isValid", &jsIsValid, 0);
        setupMethod(ctx, obj, "path", &jsPath, 0);
        setupMethod(ctx, obj, "remove", &jsRemove, 0);
    }

private:
    TemporaryDir(JSContext *) { m_dir.setAutoRemove(false); }

    DEFINE_JS_FORWARDER(jsIsValid, &TemporaryDir::isValid, "TemporaryDir.isValid")
    DEFINE_JS_FORWARDER(jsPath, &TemporaryDir::path, "TemporaryDir.path")
    DEFINE_JS_FORWARDER(jsRemove, &TemporaryDir::remove, "TemporaryDir.remove")

    bool isValid() const { return m_dir.isValid(); }

    QString path() const
    {
        return tempDirIsCanonical() ? m_dir.path() : QFileInfo(m_dir.path()).canonicalFilePath();
    }

    bool remove() { return m_dir.remove(); }

    QTemporaryDir m_dir;
};

} // namespace Internal
} // namespace qbs

void initializeJsExtensionTemporaryDir(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::TemporaryDir::registerClass(engine, extensionObject);
}
