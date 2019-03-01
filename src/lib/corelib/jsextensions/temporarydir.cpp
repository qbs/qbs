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

#include <language/scriptengine.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qobject.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

static bool tempDirIsCanonical()
{
#if QT_VERSION >= 0x050c00
    return true;
#endif
    return false;
}

class TemporaryDir : public QObject, public QScriptable
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    TemporaryDir(QScriptContext *context);
    Q_INVOKABLE bool isValid() const;
    Q_INVOKABLE QString path() const;
    Q_INVOKABLE bool remove();
private:
    QTemporaryDir dir;
};

QScriptValue TemporaryDir::ctor(QScriptContext *context, QScriptEngine *engine)
{
    const auto se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    });
    se->checkContext(QStringLiteral("qbs.TemporaryDir"), dubiousContexts);

    const auto t = new TemporaryDir(context);
    QScriptValue obj = engine->newQObject(t, QScriptEngine::ScriptOwnership);
    return obj;
}

TemporaryDir::TemporaryDir(QScriptContext *context)
{
    Q_UNUSED(context);
    dir.setAutoRemove(false);
}

bool TemporaryDir::isValid() const
{
    return dir.isValid();
}

QString TemporaryDir::path() const
{
    return tempDirIsCanonical() ? dir.path() : QFileInfo(dir.path()).canonicalFilePath();
}

bool TemporaryDir::remove()
{
    return dir.remove();
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionTemporaryDir(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&TemporaryDir::staticMetaObject,
                                              engine->newFunction(&TemporaryDir::ctor));
    extensionObject.setProperty(QStringLiteral("TemporaryDir"), obj);
}

#include "temporarydir.moc"
