/****************************************************************************
**
** Copyright (C) 2024 Raphaël Cotty <raphael.cotty@gmail.com>
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.TextFile

import "cmakeexporter.js" as HelperFunctions

Module {
    property string configFileName: packageName + "Config.cmake"
    property string versionFileName: packageName + "ConfigVersion.cmake"
    property string packageName: product.targetName

    additionalProductTypes: ["Exporter.cmake.package"]

    Rule {
        multiplex: true
        requiresInputs: false

        auxiliaryInputs: {
            if (product.type.includes("staticlibrary"))
                return ["staticlibrary"];
            if (product.type.includes("dynamiclibrary"))
                return ["dynamiclibrary"];
        }

        Artifact {
            filePath: product.Exporter.cmake.configFileName
            fileTags: ["Exporter.cmake.package", "Exporter.cmake.configFile"]
        }
        Artifact {
            filePath: product.Exporter.cmake.versionFileName
            fileTags: ["Exporter.cmake.package", "Exporter.cmake.versionFile"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generate cmake package files";
            cmd.sourceCode = function() {
                HelperFunctions.writeConfigFile(project, product, outputs);
                HelperFunctions.writeVersionFile(product, outputs);
            }
            return [cmd];
        }
    }
}
