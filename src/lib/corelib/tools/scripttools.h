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

#ifndef QBS_SCRIPTTOOLS_H
#define QBS_SCRIPTTOOLS_H

#include <tools/qbs_export.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptprogram.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

template <typename C>
QScriptValue toScriptValue(QScriptEngine *scriptEngine, const C &container)
{
    QScriptValue v = scriptEngine->newArray(container.size());
    int i = 0;
    for (const typename C::value_type &item : container)
        v.setProperty(i++, scriptEngine->toScriptValue(item));
    return v;
}

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value);
QVariant QBS_AUTOTEST_EXPORT getConfigProperty(const QVariantMap &cfg, const QStringList &name);

template <class T>
void attachPointerTo(QScriptValue &scriptValue, T *ptr)
{
    QVariant v;
    v.setValue<quintptr>(reinterpret_cast<quintptr>(ptr));
    scriptValue.setData(scriptValue.engine()->newVariant(v));
}

template <class T>
T *attachedPointer(const QScriptValue &scriptValue)
{
    const auto ptr = scriptValue.data().toVariant().value<quintptr>();
    return reinterpret_cast<T *>(ptr);
}

class TemporaryGlobalObjectSetter
{
public:
    TemporaryGlobalObjectSetter(const QScriptValue &object);
    ~TemporaryGlobalObjectSetter();

private:
    QScriptValue m_oldGlobalObject;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTTOOLS_H
