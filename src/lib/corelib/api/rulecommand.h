/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#ifndef QBS_RULECOMMAND_H
#define QBS_RULECOMMAND_H

#include <tools/qbs_export.h>

#include <QExplicitlySharedDataPointer>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class ProjectPrivate;
class RuleCommandPrivate;
}

class QBS_EXPORT RuleCommand
{
    friend class Internal::ProjectPrivate;
public:
    RuleCommand();
    RuleCommand(const RuleCommand &other);
    ~RuleCommand();
    RuleCommand &operator=(const RuleCommand &other);

    enum Type { ProcessCommandType, JavaScriptCommandType, InvalidType };


    Type type() const;
    QString description() const;
    QString sourceCode() const;
    QString executable() const;
    QStringList arguments() const;
    QString workingDirectory() const;
    QProcessEnvironment environment() const;

private:
    QExplicitlySharedDataPointer<Internal::RuleCommandPrivate> d;
};


typedef QList<RuleCommand> RuleCommandList;

} // namespace qbs

#endif // Include guard.
