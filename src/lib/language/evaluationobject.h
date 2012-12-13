/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef EVALUATIONOBJECT_H
#define EVALUATIONOBJECT_H

#include "scope.h"
#include "modules.h"
#include <QByteArray>
#include <QHash>
#include <QList>

namespace qbs {
namespace Internal {

class LanguageObject;

class EvaluationObject
{
    Q_DISABLE_COPY(EvaluationObject)
public:
    EvaluationObject(LanguageObject *instantiatingObject);
    ~EvaluationObject();

    LanguageObject *instantiatingObject() const;
    void dump(QByteArray &indent);

    QString prototype;

    Scope::Ptr scope;
    QList<EvaluationObject *> children;
    QHash<QString, Module::Ptr> modules;
    QList<UnknownModule::Ptr> unknownModules;

    // the source objects that generated this object
    // the object that triggered the instantiation is first, followed by prototypes
    QList<LanguageObject *> objects;
};

} // namespace Internal
} // namespace qbs

#endif // EVALUATIONOBJECT_H
