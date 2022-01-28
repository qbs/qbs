/****************************************************************************
**
** Copyright (C) 2020 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.FileInfo
import qbs.ModUtils
import qbs.Utilities
import "path-probe.js" as PathProbeConfigure

BinaryProbe {
    // input
    property string preferredArchitecture

    configure: {
        var _selectors;
        var results = PathProbeConfigure.configure(_selectors, names, nameSuffixes, nameFilter,
                                                   candidateFilter, searchPaths, pathSuffixes,
                                                   platformSearchPaths, environmentPaths,
                                                   platformEnvironmentPaths);
        if (!results.found) {
            var msvcs = Utilities.installedMSVCs(preferredArchitecture);
            if (msvcs.length >= 1) {
                var result = {};
                result.fileName = "cl.exe";
                result.path = msvcs[0].binPath;
                result.filePath = FileInfo.joinPaths(path, fileName);
                result.candidatePaths = result.filePath;
                results.found = true;
                results.files = [result];
            }
        }

        found = results.found;
        allResults = results.files;

        if (allResults.length === 1) {
            var result = allResults[0];
            candidatePaths = result.candidatePaths;
            path = result.path;
            filePath = result.filePath;
            fileName = result.fileName;
        }

    }
}
