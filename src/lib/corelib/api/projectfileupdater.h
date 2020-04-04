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
#ifndef QBS_PROJECTFILEUPDATER_H
#define QBS_PROJECTFILEUPDATER_H

#include "projectdata.h"

#include <tools/error.h>
#include <tools/codelocation.h>

#include <QtCore/qstringlist.h>

namespace QbsQmlJS { namespace AST { class UiProgram; } }

namespace qbs {
namespace Internal {

class ProjectFileUpdater
{
public:
    virtual ~ProjectFileUpdater();
    void apply();

    CodeLocation itemPosition() const { return m_itemPosition; }
    int lineOffset() const { return m_lineOffset; }

protected:
    ProjectFileUpdater(QString projectFile);

    QString projectFile() const { return m_projectFile; }

    void setLineOffset(int offset) { m_lineOffset = offset; }
    void setItemPosition(const CodeLocation &cl) { m_itemPosition = cl; }

private:
    virtual void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) = 0;

    enum LineEndingType
    {
        UnknownLineEndings,
        UnixLineEndings,
        WindowsLineEndings,
        MixedLineEndings
    };

    static LineEndingType guessLineEndingType(const QByteArray &text);
    static void convertToUnixLineEndings(QByteArray *text, LineEndingType oldLineEndings);
    static void convertFromUnixLineEndings(QByteArray *text, LineEndingType newLineEndings);

    const QString m_projectFile;
    CodeLocation m_itemPosition;
    int m_lineOffset = 0;
};


class ProjectFileGroupInserter : public ProjectFileUpdater
{
public:
    ProjectFileGroupInserter(ProductData product, QString groupName);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) override;

    const ProductData m_product;
    const QString m_groupName;
};


class ProjectFileFilesAdder : public ProjectFileUpdater
{
public:
    ProjectFileFilesAdder(ProductData product, GroupData group, QStringList files);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) override;

    const ProductData m_product;
    const GroupData m_group;
    const QStringList m_files;
};

class ProjectFileFilesRemover : public ProjectFileUpdater
{
public:
    ProjectFileFilesRemover(ProductData product, GroupData group,
                            QStringList files);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) override;

    const ProductData m_product;
    const GroupData m_group;
    const QStringList m_files;
};

class ProjectFileGroupRemover : public ProjectFileUpdater
{
public:
    ProjectFileGroupRemover(ProductData product, GroupData group);

private:
    void doApply(QString &fileContent, QbsQmlJS::AST::UiProgram *ast) override;

    const ProductData m_product;
    const GroupData m_group;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
