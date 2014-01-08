/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QBS_PROJECTFILEUPDATER_H
#define QBS_PROJECTFILEUPDATER_H

#include "projectdata.h"

#include <tools/error.h>
#include <tools/codelocation.h>

#include <QStringList>

namespace QbsQmlJS { namespace AST { class UiProgram; } }

namespace qbs {
namespace Internal {

class ProjectFileUpdater
{
public:
    void apply();

    CodeLocation itemPosition() const { return m_itemPosition; }
    int lineOffset() const { return m_lineOffset; }

protected:
    ProjectFileUpdater(const QString &projectFile);

    QString projectFile() const { return m_projectFile; }

    void setLineOffset(int offset) { m_lineOffset = offset; }
    void setItemPosition(const CodeLocation &cl) { m_itemPosition = cl; }

private:
    virtual void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) = 0;

    const QString m_projectFile;
    CodeLocation m_itemPosition;
    int m_lineOffset;
};


class ProjectFileGroupInserter : public ProjectFileUpdater
{
public:
    ProjectFileGroupInserter(const ProductData &product, const QString &groupName);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast);

    const ProductData m_product;
    const QString m_groupName;
};


class ProjectFileFilesAdder : public ProjectFileUpdater
{
public:
    ProjectFileFilesAdder(const ProductData &product, const GroupData &group,
                          const QStringList &files);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast);

    const ProductData m_product;
    const GroupData m_group;
    const QStringList m_files;
};

class ProjectFileFilesRemover : public ProjectFileUpdater
{
public:
    ProjectFileFilesRemover(const ProductData &product, const GroupData &group,
                            const QStringList &files);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast);

    const ProductData m_product;
    const GroupData m_group;
    const QStringList m_files;
};

class ProjectFileGroupRemover : public ProjectFileUpdater
{
public:
    ProjectFileGroupRemover(const ProductData &product, const GroupData &group);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast);

    const ProductData m_product;
    const GroupData m_group;
};

} // namespace Internal
} // namespace qbs


#endif // Include guard.
