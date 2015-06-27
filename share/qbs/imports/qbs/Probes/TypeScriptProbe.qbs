/****************************************************************************
**
** Copyright (C) 2015 Jake Petroules.
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
import qbs.ModUtils
import "../../../modules/typescript/typescript.js" as TypeScript

BinaryProbe {
    id: tsc
    names: ["tsc"]

    // Inputs
    property path nodejsToolchainInstallPath

    // Outputs
    property var version

    configure: {
        if (!nodejsToolchainInstallPath)
            throw '"nodejsToolchainInstallPath" must be specified';
        // HACK: Duplicated from PathProbe.qbs
        if (!names)
            throw '"names" must be specified';
        var _names = ModUtils.concatAll(names);
        if (nameFilter)
            _names = _names.map(nameFilter);
        // FIXME: Suggest how to obtain paths from system
        var _paths = ModUtils.concatAll(pathPrefixes, platformPaths);
        // FIXME: Add getenv support
        var envs = ModUtils.concatAll(platformEnvironmentPaths, environmentPaths);
        for (var i = 0; i < envs.length; ++i) {
            var value = qbs.getEnv(envs[i]) || '';
            if (value.length > 0)
                _paths = _paths.concat(value.split(qbs.pathListSeparator));
        }
        var _suffixes = ModUtils.concatAll('', pathSuffixes);
        for (i = 0; i < _names.length; ++i) {
            for (var j = 0; j < _paths.length; ++j) {
                for (var k = 0; k < _suffixes.length; ++k) {
                    var _filePath = FileInfo.joinPaths(_paths[j], _suffixes[k], _names[i]);
                    if (File.exists(_filePath)) {
                        found = true;
                        filePath = File.canonicalFilePath(_filePath);
                        fileName = FileInfo.fileName(filePath);
                        path = FileInfo.path(filePath);
                        version = TypeScript.findTscVersion(filePath, nodejsToolchainInstallPath);
                        return;
                    }
                }
            }
        }
        found = false;
        path = undefined;
        filePath = undefined;
        fileName = undefined;
        version = undefined;
        // HACK: Duplicated from PathProbe.qbs
    }
}
