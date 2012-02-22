/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
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


#ifndef QBS_QBSBUILDPRODUCT_H
#define QBS_QBSBUILDPRODUCT_H

#include "private/resolvedproduct.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QStringList>
#include <QtCore/QSet>

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
    QString displayName() const;
    QString filePath() const;
    QVector<SourceFile> sourceFiles() const;
    QStringList projectIncludePaths() const;
    QString executablePath() const;

    bool isExecutable() const;

    Private::ResolvedProduct privateResolvedProject() const;

    void dump();

    QSharedPointer<qbs::BuildProduct> internalBuildProduct() const;

private:  // functions
    BuildProduct(const QSharedPointer<qbs::BuildProduct> &internalBuildProduct);

private:  // variables
    QSharedPointer<qbs::BuildProduct> m_internalBuildProduct;
};

} // namespace Qbs

#endif // QBS_QBSBUILDPRODUCT_H
