/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

import "qbs-pkg-config-probe.js" as PkgConfigProbeConfigure

Probe {
    // Inputs

    property string _executableFilePath
    property stringList _extraPaths
    property stringList _libDirs
    property bool _staticMode: false
    property bool _definePrefix: false

    property path _sysroot

    // Output
    property var packages
    property var packagesByModuleName
    property var brokenPackages
    property stringList qmakePaths

    configure: {
        var result = PkgConfigProbeConfigure.configure(
            _executableFilePath,
            _extraPaths,
            _libDirs,
            _staticMode,
            _definePrefix,
            _sysroot);
        packages = result.packages;
        packagesByModuleName = result.packagesByModuleName;
        brokenPackages = result.brokenPackages;
        qmakePaths = result.qmakePaths;
        found = true;
    }
}
