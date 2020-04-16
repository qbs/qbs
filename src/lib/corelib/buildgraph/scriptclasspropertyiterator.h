/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QBS_SCRIPTCLASSPROPERTYITERATOR_H
#define QBS_SCRIPTCLASSPROPERTYITERATOR_H

#include <tools/qbsassert.h>

#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtScript/qscriptclasspropertyiterator.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptstring.h>

#include <vector>

namespace qbs {
namespace Internal {

class ScriptClassPropertyIterator : public QScriptClassPropertyIterator
{
public:
    ScriptClassPropertyIterator(const QScriptValue &object, const QVariantMap &properties,
                                std::vector<QString> additionalProperties)
        : QScriptClassPropertyIterator(object),
          m_it(properties),
          m_additionalProperties(std::move(additionalProperties))
    {
    }

private:
    bool hasNext() const override
    {
        return m_it.hasNext() || m_index < int(m_additionalProperties.size()) - 1;
    }
    bool hasPrevious() const override { return m_index > -1 || m_it.hasPrevious(); }
    void toFront() override { m_it.toFront(); m_index = -1; }
    void toBack() override { m_it.toBack(); m_index = int(m_additionalProperties.size()) - 1; }

    void next() override
    {
        QBS_ASSERT(hasNext(), return);
        if (m_it.hasNext())
            m_it.next();
        else
            ++m_index;
    }

    void previous() override
    {
        QBS_ASSERT(hasPrevious(), return);
        if (m_index >= 0)
            --m_index;
        if (m_index == -1)
            m_it.previous();
    }

    QScriptString name() const override
    {
        const QString theName = m_index >= 0 && m_index < int(m_additionalProperties.size())
                ? m_additionalProperties.at(m_index)
                : m_it.key();
        return object().engine()->toStringHandle(theName);
    }

    QMapIterator<QString, QVariant> m_it;
    const std::vector<QString> m_additionalProperties;
    int m_index = -1;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTCLASSPROPERTYITERATOR_H
