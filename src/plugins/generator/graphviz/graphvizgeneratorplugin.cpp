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

#include "graphvizgenerator.h"

#include <tools/projectgeneratormanager.h>
#include <tools/qbspluginmanager.h>

static void GraphvizPluginLoad()
{
    qbs::ProjectGeneratorManager::registerGenerator(std::make_shared<qbs::GraphvizGenerator>());
}

static void GraphvizPluginUnload() {}

#ifndef GENERATOR_EXPORT
#if defined(WIN32) || defined(_WIN32)
#define GENERATOR_EXPORT __declspec(dllexport)
#else
#define GENERATOR_EXPORT __attribute__((visibility("default")))
#endif
#endif

QBS_REGISTER_STATIC_PLUGIN(
    extern "C" GENERATOR_EXPORT, graphviz, GraphvizPluginLoad, GraphvizPluginUnload)
