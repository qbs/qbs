/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");

function asStringList(key, value) {
    if (typeof(value) === "string")
        return [value];
    if (value instanceof Array)
        return value;
    throw key + " must be a string or a stringList";
}

function canonicalSelectors(selectors, nameSuffixes) {
    var mapper = function(selector) {
        if (typeof(selector) === "string")
            return {names : [selector]};
        if (selector instanceof Array)
            return {names : selector};
        // dict
        if (!selector.names)
            throw '"names" must be specified';
        selector.names = asStringList("names", selector.names);
        if (selector.nameSuffixes)
            selector.nameSuffixes = asStringList("nameSuffixes", selector.nameSuffixes);
        else
            selector.nameSuffixes = nameSuffixes;
        return selector;
    };
    return selectors.map(mapper);
}

function pathsFromEnvs(envs, pathListSeparator) {
    envs = envs || [];
    var result = [];
    for (var i = 0; i < envs.length; ++i) {
        var value = Environment.getEnv(envs[i]) || '';
        if (value.length > 0)
            result = result.concat(value.split(pathListSeparator));
    }
    return result;
}

function configure(selectors, names, nameSuffixes, nameFilter, candidateFilter,
                   searchPaths, pathSuffixes, platformSearchPaths, environmentPaths,
                   platformEnvironmentPaths) {
    var result = { found: false, files: [] };
    if (!selectors && !names)
        throw '"names" or "selectors" must be specified';

    if (!selectors) {
        selectors = [
            {names: names, nameSuffixes: nameSuffixes}
        ];
    } else {
        selectors = canonicalSelectors(selectors, nameSuffixes);
    }

    if (nameFilter) {
        selectors.forEach(function(selector) {
            selector.names = selector.names.map(nameFilter);
        });
    }

    selectors.forEach(function(selector) {
        selector.names = ModUtils.concatAll.apply(undefined, selector.names.map(function(name) {
            return (selector.nameSuffixes || [""]).map(function(suffix) {
                return name + suffix;
            });
        }));
    });

    // FIXME: Suggest how to obtain paths from system
    var _paths = ModUtils.concatAll(
                pathsFromEnvs(environmentPaths, FileInfo.pathListSeparator()),
                searchPaths,
                pathsFromEnvs(platformEnvironmentPaths, FileInfo.pathListSeparator()),
                platformSearchPaths);
    var _suffixes = ModUtils.concatAll('', pathSuffixes);
    _paths = _paths.map(function(p) { return FileInfo.fromNativeSeparators(p); });
    _suffixes = _suffixes.map(function(p) { return FileInfo.fromNativeSeparators(p); });

    var findFile = function(selector) {
        var file = { found: false, candidatePaths: [] };
        for (var i = 0; i < selector.names.length; ++i) {
            for (var j = 0; j < _paths.length; ++j) {
                for (var k = 0; k < _suffixes.length; ++k) {
                    var _filePath = FileInfo.joinPaths(_paths[j], _suffixes[k], selector.names[i]);
                    file.candidatePaths.push(_filePath);
                    if (File.exists(_filePath)
                            && (!candidateFilter || candidateFilter(_filePath))) {
                        file.found = true;
                        file.filePath = _filePath;

                        // Manually specify the path components that constitute _filePath rather
                        // than using the FileInfo.path and FileInfo.fileName functions because we
                        // want to break _filePath into its constituent parts based on the input
                        // originally given by the user. For example, the FileInfo functions would
                        // produce a different result if any of the items in the names property
                        // contained more than a single path component.
                        file.fileName = selector.names[i];
                        file.path = FileInfo.joinPaths(_paths[j], _suffixes[k]);
                        return file;
                    }
                }
            }
        }

        return file;
    };

    result.files = selectors.map(findFile);
    result.found = result.files.reduce(function(acc, value) { return acc && value.found }, true);

    return result;
}
