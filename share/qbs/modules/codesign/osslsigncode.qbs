/****************************************************************************
**
** Copyright (C) 2026 Roman Telezhynskyi <dismine@gmail.com>
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

CodeSignModule {
    condition: qbs.targetOS.includes("windows")
               && !Host.os().includes("windows")

    // Requires osslsigncode 2.0 or later, which introduced the command-based CLI (the "sign"
    // sub-command) that the signing rule relies on.
    _canSignArtifacts: true

    Probes.BinaryProbe {
        id: osslsigncodeProbe
        condition: !codesignPath
        names: [codesignName]
    }

    codesignName: "osslsigncode"
    codesignPath: osslsigncodeProbe.found ? osslsigncodeProbe.filePath : undefined

    property string hashAlgorithm
    PropertyOptions {
        name: "hashAlgorithm"
        description: "Name of the cryptographic hash function used when signing."
        allowedValues: ["sha1", "sha256", "sha384", "sha512"]
    }

    property path certificatePath
    PropertyOptions {
        name: "certificatePath"
        description: "Path to the signing certificate PKCS#12 (PFX) file."
    }

    property string certificatePassword
    PropertyOptions {
        name: "certificatePassword"
        description: "Password to use when opening the certificate PKCS#12 (PFX) file."
    }

    validate: {
        if (!enableCodeSigning)
            return;
        if (!File.exists(codesignPath))
            throw ModUtils.ModuleError("Could not find 'osslsigncode' utility");
        if (!certificatePath) {
            throw ModUtils.ModuleError("codesign.certificatePath must be set to a PKCS#12 (PFX) "
                                       + "file when signing with osslsigncode");
        }
    }
}
