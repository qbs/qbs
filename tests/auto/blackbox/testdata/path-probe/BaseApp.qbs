/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com).
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
import qbs.Probes

CppApplication {

    property varList inputSelectors
    property varList inputNames
    property varList inputNameSuffixes
    property pathList inputSearchPaths
    property var inputNameFilter
    property var inputCandidateFilter

    property stringList outputFilePaths
    property var outputCandidatePaths

    Probes.PathProbe {
        id: probe
        selectors: inputSelectors
        names: inputNames
        nameSuffixes: inputNameSuffixes
        nameFilter: inputNameFilter
        candidateFilter: inputCandidateFilter
        searchPaths: inputSearchPaths
        platformSearchPaths: []
    }

    property bool validate: {
        var compareArrays = function(lhs, rhs) {
            if (lhs.length !== rhs.length)
                return false;
            for (var i = 0; i < lhs.length; ++i) {
                if (Array.isArray(lhs[i]) && Array.isArray(rhs[i])) {
                    if (!compareArrays(lhs[i], rhs[i]))
                        return false;
                } else if (lhs[i] !== rhs[i]) {
                    return false;
                }
            }
            return true;
        };

        if (outputCandidatePaths) {
            var actual = probe.allResults.map(function(file) { return file.candidatePaths; });
            if (!compareArrays(actual, outputCandidatePaths)) {
                throw "Invalid canndidatePaths: actual = " + JSON.stringify(actual)
                        + ", expected = " + JSON.stringify(outputCandidatePaths);
            }
        }

        if (!probe.found) {
            if (probe.filePath) {
                throw "Invalid filePath: actual = " + JSON.stringify(probe.filePath)
                        + ", expected = 'undefined'";
            }
            if (probe.fileName) {
                throw "Invalid fileName: actual = " + JSON.stringify(probe.fileName)
                        + ", expected = 'undefined'";
            }
            if (probe.path) {
                throw "Invalid path: actual = " + JSON.stringify(probe.path)
                        + ", expected = 'undefined'";
            }

            throw "Probe failed to find files";
        }

        if (outputFilePaths) {
            var actual = probe.allResults.map(function(file) { return file.filePath; });
            if (!compareArrays(actual, outputFilePaths)) {
                throw "Invalid filePaths: actual = " + JSON.stringify(actual)
                        + ", expected = " + JSON.stringify(outputFilePaths);
            }
        }

        if (probe.allResults.length !== 1)
            return;

        // check that single-file interface matches the first value in allResults
        var expectedFilePath = probe.allResults[0].filePath;
        if (probe.filePath !== expectedFilePath) {
            throw "Invalid filePath: actual = " + probe.filePath
                    + ", expected = " + expectedFilePath;
        }
        var expectedFileName = probe.allResults[0].fileName;
        if (probe.fileName !== expectedFileName) {
            throw "Invalid fileName: actual = " + probe.fileName
                    + ", expected = " + expectedFileName;
        }
        var expectedPath = probe.allResults[0].path;
        if (probe.path !== expectedPath) {
            throw "Invalid path: actual = " + probe.path
                    + ", expected = " + expectedPath;
        }
        var expectedCandidatePaths = probe.allResults[0].candidatePaths;
        if (!compareArrays(probe.candidatePaths, expectedCandidatePaths)) {
            throw "Invalid candidatePaths: actual = " + JSON.stringify(probe.candidatePaths)
                    + ", expected = " + JSON.stringify(expectedCandidatePaths);
        }
    }

    files: ["main.cpp"]
}
