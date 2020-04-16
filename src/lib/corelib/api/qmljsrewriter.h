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

#ifndef QMLJSREWRITER_H
#define QMLJSREWRITER_H

#include "changeset.h"

#include <parser/qmljsastfwd_p.h>

#include <QtCore/qstringlist.h>

namespace QbsQmlJS {

class Rewriter
{
public:
    enum BindingType {
        ScriptBinding,
        ObjectBinding,
        ArrayBinding
    };

    using Range = ChangeSet::Range;

public:
    Rewriter(QString originalText,
             ChangeSet *changeSet,
             QStringList propertyOrder);

    Range addBinding(AST::UiObjectInitializer *ast,
                     const QString &propertyName,
                     const QString &propertyValue,
                     BindingType bindingType);

    Range addBinding(AST::UiObjectInitializer *ast,
                     const QString &propertyName,
                     const QString &propertyValue,
                     BindingType bindingType,
                     AST::UiObjectMemberList *insertAfter);

    void changeBinding(AST::UiObjectInitializer *ast,
                       const QString &propertyName,
                       const QString &newValue,
                       BindingType binding);

    void removeBindingByName(AST::UiObjectInitializer *ast, const QString &propertyName);

    void appendToArrayBinding(AST::UiArrayBinding *arrayBinding,
                              const QString &content);

    Range addObject(AST::UiObjectInitializer *ast, const QString &content);
    Range addObject(AST::UiObjectInitializer *ast, const QString &content, AST::UiObjectMemberList *insertAfter);
    Range addObject(AST::UiArrayBinding *ast, const QString &content);
    Range addObject(AST::UiArrayBinding *ast, const QString &content, AST::UiArrayMemberList *insertAfter);

    void removeObjectMember(AST::UiObjectMember *member, AST::UiObjectMember *parent);

    static AST::UiObjectMemberList *searchMemberToInsertAfter(AST::UiObjectMemberList *members, const QStringList &propertyOrder);
    static AST::UiArrayMemberList *searchMemberToInsertAfter(AST::UiArrayMemberList *members, const QStringList &propertyOrder);
    static AST::UiObjectMemberList *searchMemberToInsertAfter(AST::UiObjectMemberList *members, const QString &propertyName, const QStringList &propertyOrder);

    static bool includeSurroundingWhitespace(const QString &source, int &start, int &end);
    static void includeLeadingEmptyLine(const QString &source, int &start);
    static void includeEmptyGroupedProperty(AST::UiObjectDefinition *groupedProperty, AST::UiObjectMember *memberToBeRemoved, int &start, int &end);

private:
    void replaceMemberValue(AST::UiObjectMember *propertyMember,
                            const QString &newValue,
                            bool needsSemicolon);
    static bool isMatchingPropertyMember(const QString &propertyName,
                                         AST::UiObjectMember *member);
    static bool nextMemberOnSameLine(AST::UiObjectMemberList *members);

    void insertIntoArray(AST::UiArrayBinding* ast, const QString &newValue);

    void removeMember(AST::UiObjectMember *member);
    void removeGroupedProperty(AST::UiObjectDefinition *ast,
                               const QString &propertyName);

    void extendToLeadingOrTrailingComma(AST::UiArrayBinding *parentArray,
                                        AST::UiObjectMember *member,
                                        int &start,
                                        int &end) const;

private:
    QString m_originalText;
    ChangeSet *m_changeSet;
    const QStringList m_propertyOrder;
};

} // namespace QbsQmlJS

#endif // QMLJSREWRITER_H
