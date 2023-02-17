/****************************************************************************
**
** Copyright (C) 2021 Denis Shienkov <denis.shienkov@gmail.com>
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
import "codesign.js" as CODESIGN

CodeSignModule {
    condition: qbs.targetOS.includes("windows")
               && Host.os().includes("windows")
               && qbs.toolchain.includes("msvc")

    _canSignArtifacts: true

    Probes.BinaryProbe {
        id: signtoolProbe
        names: [codesignName]
        searchPaths: CODESIGN.findBestSignToolSearchPaths(Host.architecture())
    }

    codesignName: "signtool"
    codesignPath: signtoolProbe.filePath

    property string subjectName
    PropertyOptions {
        name: "subjectName"
        description: "Name of the subject of the signing certificate."
    }

    property string rootSubjectName
    PropertyOptions {
        name: "rootSubjectName"
        description: "Name of the subject of the root certificate that the signing " +
                     "certificate must chain to."
    }

    property string hashAlgorithm
    PropertyOptions {
        name: "hashAlgorithm"
        description: "Name of the hash algorithm used on the signing certificate."
        allowedValues: ["sha1", "sha256", "sha384", "sha512"]
    }

    property string timestampAlgorithm
    PropertyOptions {
        name: "timestampAlgorithm"
        description: "Name of the timestamp algorithm."
        allowedValues: ["sha1", "sha256"]
    }

    property path certificatePath
    PropertyOptions {
        name: "certificatePath"
        description: "Path to the signing certificate PFX file."
    }

    property string certificatePassword
    PropertyOptions {
        name: "certificatePassword"
        description: "Password to use when opening a certificate PFX file."
    }

    property path crossCertificatePath
    PropertyOptions {
        name: "crossCertificatePath"
        description: "Path to the additional certificate CER file."
    }

    validate: {
        if (enableCodeSigning && !File.exists(codesignPath)) {
            throw ModUtils.ModuleError("Could not find 'signtool' utility");
        }
    }
}
