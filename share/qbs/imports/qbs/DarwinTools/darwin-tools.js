/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

var FileInfo = loadExtension("qbs.FileInfo");

/**
  * Returns the most appropriate Apple platform name given a targetOS list.
  * Possible platform names include macosx, iphoneos, and iphonesimulator.
  */
function applePlatformName(targetOSList) {
    if (targetOSList.contains("ios-simulator"))
        return "iphonesimulator";
    else if (targetOSList.contains("ios"))
        return "iphoneos";
    else if (targetOSList.contains("osx"))
        return "macosx";
    throw("No Apple platform corresponds to target OS list: " + targetOSList);
}

/**
  * Replace characters unsafe for use in a domain name with a '-' character (RFC 1034).
  */
function rfc1034(inStr) {
    // ### Remove in Qbs 1.5
    print("WARNING: DarwinTools.rfc1034 is deprecated; use qbs.rfc1034Identifier instead");
    return qbs.rfc1034Identifier(inStr);
}

/**
  * Returns the localization of the resource at the given path,
  * or undefined if the path does not contain an {xx}.lproj path segment.
  */
function localizationKey(path) {
    return _resourceFileProperties(path)[0];
}

/**
  * Returns the path of a localized resource at the given path,
  * relative to its containing {xx}.lproj directory, or '.'
  * if the resource is unlocalized (not contained in an lproj directory).
  */
function relativeResourcePath(path) {
    return _resourceFileProperties(path)[1];
}

function _resourceFileProperties(path) {
    var lprojKey = ".lproj/";
    var lproj = path.indexOf(lprojKey);
    if (lproj >= 0) {
        // The slash preceding XX.lproj
        var slashIndex = path.slice(0, lproj).lastIndexOf('/');
        if (slashIndex >= 0) {
            var localizationKey = path.slice(slashIndex + 1, lproj);
            var relativeResourcePath = FileInfo.path(path.slice(lproj + lprojKey.length));
            return [ localizationKey, relativeResourcePath ];
        }
    }

    return [ undefined, '.' ];
}

/**
  * Recursively perform variable replacements in an environment dictionary.
  *
  * JSON.stringify(expandPlistEnvironmentVariables({a:"$(x)3$$(y)",b:{t:"%$(y) $(k)"}},
  *                                                {x:"X",y:"Y"}, true))
  *    Warning undefined variable  k  in variable expansion
  * => {"a":"X3$Y","b":{"t":"%Y $(k)"}}
  */
function expandPlistEnvironmentVariables(obj, env, warn) {
    // Possible syntaxes for wrapping an environment variable name
    var syntaxes = [
        {"open": "${", "close": "}"},
        {"open": "$(", "close": ")"},
        {"open": "@",  "close": "@"}
    ];

    /**
      * Finds the first index of a replacement starting with one of the supported syntaxes
      * This is needed so we don't do recursive substitutions
      */
    function indexOfReplacementStart(syntaxes, str, offset) {
        var syntax;
        var idx = str.length;
        for (var i in syntaxes) {
            var j = str.indexOf(syntaxes[i].open, offset);
            if (j !== -1 && j < idx) {
                syntax = syntaxes[i];
                idx = j;
            }
        }
        return { "syntax": syntax, "index": idx === str.length ? -1 : idx };
    }

    function expandRecursive(obj, env, checked) {
        checked.push(obj);
        for (var key in obj) {
            var value = obj[key];
            var type = typeof(value);
            if (type === "object") {
                if (checked.indexOf(value) !== -1)
                    continue;
                expandRecursive(value, env, checked);
            }
            if (type !== "string")
                continue;
            var repl = indexOfReplacementStart(syntaxes, value);
            var i = repl.index;
            var changes = false;
            while (i !== -1) {
                var j = value.indexOf(repl.syntax.close, i + repl.syntax.open.length);
                if (j === -1)
                    break;
                var varParts = value.slice(i + repl.syntax.open.length, j).split(':');
                var varName = varParts[0];
                var varFormatter = varParts[1];
                var varValue = env[varName];
                if (undefined === varValue) {
                    // skip replacement
                    if (warn)
                        print("Warning undefined variable ",varName, " in variable expansion");
                } else {
                    changes = true;
                    varValue = String(varValue);
                    if (varFormatter !== undefined)
                        varFormatter = varFormatter.toLowerCase();
                    if (varFormatter === "rfc1034identifier")
                        varValue = qbs.rfc1034Identifier(varValue);
                    value = value.slice(0, i) + varValue + value.slice(j + repl.syntax.close.length);
                    // avoid recursive substitutions to avoid potentially infinite loops
                    i += varValue.length;
                }
                repl = indexOfReplacementStart(syntaxes, value, i + repl.syntax.close.length);
                i = repl.index;
            }
            if (changes)
                obj[key] = value;
        }
    }
    expandRecursive(obj, env, []);
    return obj;
}

/**
  * Recursively removes any undefined, null, or empty string values from the property list.
  */
function cleanPropertyList(plist) {
    if (typeof(plist) !== "object")
        return;

    for (var key in plist) {
        if (plist[key] === undefined || plist[key] === null || plist[key] === "")
            delete plist[key];
        else
            cleanPropertyList(plist[key]);
    }
}
