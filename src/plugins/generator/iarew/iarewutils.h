/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#ifndef QBS_IAREWUTILS_H
#define QBS_IAREWUTILS_H

#include <qbs.h>

#include <tools/stringconstants.h>

namespace qbs {
namespace IarewUtils {

enum OutputBinaryType {
    ApplicationOutputType,
    LibraryOutputType
};

OutputBinaryType outputBinaryType(const ProductData &qbsProduct);

QString toolkitRootPath(const ProductData &qbsProduct);

QString dlibToolkitRootPath(const ProductData &qbsProduct);

QString clibToolkitRootPath(const ProductData &qbsProduct);

QString libToolkitRootPath(const ProductData &qbsProduct);

QString toolkitRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath);

QString projectRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath);

QString flagValue(const QStringList &flags, const QString &flagKey);

QVariantList flagValues(const QStringList &flags, const QString &flagKey);

QStringList cppModuleCompilerFlags(const PropertyMap &qbsProps);

QStringList cppModuleAssemblerFlags(const PropertyMap &qbsProps);

QStringList cppModuleLinkerFlags(const PropertyMap &qbsProps);

} // namespace IarewUtils
} // namespace qbs

#endif // QBS_IAREWUTILS_H
