/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef MSBUILDITEM_H
#define MSBUILDITEM_H

#include <QtCore/qobject.h>
#include "imsbuildnode.h"

#include <memory>

namespace qbs {

class IMSBuildItemGroup;
class MSBuildItemDefinitionGroup;
class MSBuildItemGroup;
class MSBuildItemPrivate;

/*!
 * \brief The MSBuildItem class represents an MSBuild Item element.
 *
 * https://msdn.microsoft.com/en-us/library/ms164283.aspx
 */
class MSBuildItem : public QObject, public IMSBuildNode
{
    Q_OBJECT
public:
    explicit MSBuildItem(const QString &name, IMSBuildItemGroup *parent = nullptr);
    ~MSBuildItem() override;

    QString name() const;
    void setName(const QString &name);

    QString include() const;
    void setInclude(const QString &include);

    void appendProperty(const QString &name, const QVariant &value);

    void accept(IMSBuildNodeVisitor *visitor) const override;

private:
    std::unique_ptr<MSBuildItemPrivate> d;
};

} // namespace qbs

#endif // MSBUILDITEM_H
