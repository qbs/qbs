/****************************************************************************
**
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.File
import qbs.Host
import qbs.ModUtils
import qbs.Probes

Module {
    Depends { name: "xcode"; required: false }

    condition: Host.os().includes("macos") && qbs.targetOS.includes("darwin")

    property bool enableNotarization: false
    PropertyOptions {
        name: "enableNotarization"
        description: "whether to notarize distributable macOS artifacts"
    }

    Probes.BinaryProbe {
        id: notarytoolProbe
        names: ["notarytool"]
    }

    Probes.BinaryProbe {
        id: staplerProbe
        names: ["stapler"]
    }

    Probes.BinaryProbe {
        id: xcrunProbe
        names: ["xcrun"]
    }

    property string notarytoolPath: notarytoolProbe.filePath
    property string staplerPath: staplerProbe.filePath
    property string xcrunPath: xcrunProbe.found ? xcrunProbe.filePath : "xcrun"

    readonly property bool _useXcrun: !notarytoolProbe.found || !staplerProbe.found

    property string keychainProfile
    PropertyOptions {
        name: "keychainProfile"
        description: "name of the keychain profile created with notarytool store-credentials"
    }

    property bool staple: true
    PropertyOptions {
        name: "staple"
        description: "whether to staple the notarization ticket to the artifact"
    }

    property stringList notarytoolFlags
    PropertyOptions {
        name: "notarytoolFlags"
        description: "additional flags passed to notarytool submit"
    }

    validate: {
        if (!enableNotarization)
            return;

        if (!_useXcrun) {
            if (!File.exists(notarytoolPath))
                throw ModUtils.ModuleError("Could not find 'notarytool' utility");
            if (!File.exists(staplerPath))
                throw ModUtils.ModuleError("Could not find 'stapler' utility");
        } else if (!xcrunProbe.found && !File.exists(xcrunPath)) {
            throw ModUtils.ModuleError("Could not find 'xcrun' utility");
        }

        if (!keychainProfile) {
            throw ModUtils.ModuleError(
                        "Set notarization.keychainProfile to a profile created with "
                        + "'notarytool store-credentials'.");
        }
    }
}
