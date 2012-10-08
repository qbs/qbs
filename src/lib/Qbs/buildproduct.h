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


#ifndef QBS_QBSBUILDPRODUCT_H
#define QBS_QBSBUILDPRODUCT_H

#include "private/resolvedproduct.h"

#include <QSet>
#include <QSharedPointer>
#include <QStringList>

namespace qbs {
    class BuildProduct;
    class RunEnvironment;
}

namespace Qbs {

class BuildProject;

class SourceFile
{
public:
    SourceFile(const QString &fileName = QString(), const QSet<QString> &tags = QSet<QString>());
    QString fileName;
    QSet<QString> tags;
};

inline bool lessThanSourceFile(const SourceFile &first, const SourceFile &second)
{
    return first.fileName < second.fileName;
}

class BuildProduct
{
    friend class BuildProject;
    friend class qbs::RunEnvironment;

public:
    BuildProduct();
    ~BuildProduct();
    BuildProduct(const BuildProduct &other);
    BuildProduct &operator =(const BuildProduct &other);

    bool isValid() const;

    QString name() const;
    QString targetName() const;
    QString displayName() const;
    QString filePath() const;
    QVector<SourceFile> sourceFiles() const;
    QStringList projectIncludePaths() const;
    QString executableSuffix() const;
    QString executablePath() const;

    bool isExecutable() const;

    Private::ResolvedProduct privateResolvedProject() const;

    void dump() const;
    void dumpProperties() const;

    QSharedPointer<qbs::BuildProduct> internalBuildProduct() const;

private:  // functions
    BuildProduct(const QSharedPointer<qbs::BuildProduct> &internalBuildProduct);

private:  // variables
    QSharedPointer<qbs::BuildProduct> m_internalBuildProduct;
};

} // namespace Qbs

#endif // QBS_QBSBUILDPRODUCT_H
