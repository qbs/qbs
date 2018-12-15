/****************************************************************************
**
** Copyright (C) 2018 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

import qbs
import qbs.File
import qbs.FileInfo
import qbs.Process
import qbs.ModUtils
import qbs.Utilities
import "../../../modules/cpp/iar.js" as IAR

PathProbe {
    // Inputs
    property string compilerFilePath;
    property string preferredArchitecture;

    property string _nullDevice: qbs.nullDevice

    // Outputs
    property string architecture;
    property string endianness;
    property int versionMajor;
    property int versionMinor;
    property int versionPatch;

    configure: {
        if (!File.exists(compilerFilePath)) {
            found = false;
            return;
        }

        var macros = IAR.dumpMacros(compilerFilePath, qbs, _nullDevice);

        architecture = IAR.guessArchitecture(macros);
        endianness = IAR.guessEndianness(macros);

        var version = parseInt(macros['__VER__'], 10);
        versionMajor = parseInt(version / 1000000);
        versionMinor = parseInt(version / 1000) % 1000;
        versionPatch = parseInt(version) % 1000;

        found = version && architecture && endianness;
   }
}
