/*************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "tools/persistence.h"

#include <QSet>
#include <QSharedPointer>
#include <QScriptEngine>

QT_BEGIN_NAMESPACE

QT_END_NAMESPACE

namespace qbs {

class Artifact;
class AbstractCommand;
class Rule;

class Transformer : public PersistentObject
{
public:
    typedef QSharedPointer<Transformer> Ptr;

    Transformer();
    ~Transformer();

    QSet<Artifact*> inputs; // can be different from "children of all outputs"
    QSet<Artifact*> outputs;
    QSharedPointer<const Rule> rule;
    QList<AbstractCommand *> commands;

    static QScriptValue translateFileConfig(QScriptEngine *scriptEngine,
                                            Artifact *artifact,
                                            const QString &defaultModuleName);
    static QScriptValue translateInOutputs(QScriptEngine *scriptEngine,
                                           const QSet<Artifact*> &artifacts,
                                           const QString &defaultModuleName);

    void setupInputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue);
    void setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue);

private:
    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

} // namespace qbs

#endif // TRANSFORMER_H
