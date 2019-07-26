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

#ifndef QBS_JSIMPORTS_H
#define QBS_JSIMPORTS_H

#include <tools/codelocation.h>
#include <tools/persistence.h>
#include <tools/qttools.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>

#include <vector>

namespace qbs {
namespace Internal {

/**
  * Represents JavaScript import of the form
  *    import 'fileOrDirectory' as scopeName
  *
  * There can be several filenames per scope
  * if we import a whole directory.
  */
class JsImport
{
public:
    QString scopeName;
    QStringList filePaths;
    CodeLocation location;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(scopeName, filePaths, location);
    }
};
inline uint qHash(const JsImport &jsi) { return qHash(jsi.scopeName); }

inline bool operator<(const JsImport &lhs, const JsImport &rhs)
{
    return lhs.scopeName < rhs.scopeName;
}

inline bool operator==(const JsImport &jsi1, const JsImport &jsi2)
{
    return jsi1.scopeName == jsi2.scopeName && toSet(jsi1.filePaths) == toSet(jsi2.filePaths);
}

using JsImports = std::vector<JsImport>;

} // namespace Internal
} // namespace qbs

#endif // QBS_JSIMPORTS_H
