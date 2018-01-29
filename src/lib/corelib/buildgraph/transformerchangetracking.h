/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QBS_TRANSFORMERCHANGETRACKING_H
#define QBS_TRANSFORMERCHANGETRACKING_H

#include "forward_decls.h"
#include <language/forward_decls.h>

#include <unordered_map>

namespace qbs {
namespace Internal {

bool prepareScriptNeedsRerun(
        Transformer *transformer,
        const ResolvedProduct *product,
        const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
        const std::unordered_map<QString, const ResolvedProject *> &projectsByName);

bool commandsNeedRerun(Transformer *transformer,
                       const ResolvedProduct *product,
                       const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
                       const std::unordered_map<QString, const ResolvedProject *> &projectsByName);

} // namespace Internal
} // namespace qbs

#endif // Include guard
