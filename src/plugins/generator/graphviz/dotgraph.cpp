/****************************************************************************
**
** Copyright (C) 2025 Ivan Komissarov (abbapoh@gmail.com).
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

#include "dotgraph.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace qbs {
namespace Internal {

void DotGraph::write(QFile &file)
{
    const auto indent = "  ";
    file.write("digraph G {\n");
    file.write(indent);
    for (const auto &node : m_nodes) {
        file.write(indent);
        file.write(node.toString());
        file.write("\n");
    }
    for (const auto &relation : m_relations) {
        file.write(indent);
        file.write(relation.toString());
        file.write("\n");
    }
    file.write("}\n");
}

void DotGraph::write(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    const auto directory = fileInfo.path();
    if (!QDir(fileInfo.path()).exists()) {
        QDir().mkpath(directory);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        throw ErrorInfo(Tr::tr("Failed to create '%1': %2").arg(filePath, file.errorString()));
    }
    write(file);
}

} // namespace Internal
} // namespace qbs
