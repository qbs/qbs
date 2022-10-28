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

var FileInfo = require("qbs.FileInfo");
var Utilities = require("qbs.Utilities");

var _deviceMap = {
    "mac": undefined, // only devices have an ID
    "iphone": 1,
    "ipad": 2,
    "tv": 3,
    "watch": 4,
    "car": 5
};

var _platformMap = {
    "ios": "iPhone",
    "macos": "MacOSX",
    "tvos": "AppleTV",
    "watchos": "Watch"
};

var _platformDeviceMap = {
    "ios": ["iphone", "ipad"],
    "macos": ["mac"],
    "tvos": ["tv"],
    "watchos": ["watch"]
}

/**
  * Returns the numeric identifier corresponding to an Apple device name
  * (i.e. for use by TARGETED_DEVICE_FAMILY).
  */
function appleDeviceNumber(deviceName) {
    return _deviceMap[deviceName];
}

/**
  * Returns the list of target devices available for the given qbs target OS list.
  */
function targetDevices(targetOS) {
    for (var key in _platformDeviceMap) {
        if (targetOS.contains(key))
            return _platformDeviceMap[key];
    }
}

/**
  * Returns the TARGETED_DEVICE_FAMILY string given a list of target device names.
  */
function targetedDeviceFamily(deviceNames) {
    return deviceNames.map(function (deviceName) {
        return appleDeviceNumber(deviceName);
    });
}

/**
  * Returns the most appropriate Apple platform name given a targetOS list.
  */
function applePlatformName(targetOSList, platformType) {
    return applePlatformDirectoryName(targetOSList, platformType).toLowerCase();
}

/**
  * Returns the most appropriate Apple platform directory name given a targetOS list and version.
  */
function applePlatformDirectoryName(targetOSList, platformType, version, throwOnError) {
    var suffixMap = {
        "device": "OS",
        "simulator": "Simulator"
    };

    for (var key in _platformMap) {
        if (targetOSList.contains(key)) {
            // there are no MacOSXOS or MacOSXSimulator platforms
            var suffix = (key !== "macos") ? (suffixMap[platformType] || "") : "";
            return _platformMap[key] + suffix + (version || "");
        }
    }

    if (throwOnError || throwOnError === undefined)
        throw("No Apple platform corresponds to target OS list: " + targetOSList);
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

var PropertyListVariableExpander = (function () {
    function PropertyListVariableExpander() {
    }
    /**
      * Recursively perform variable replacements in an environment dictionary.
      */
    PropertyListVariableExpander.prototype.expand = function (obj, env) {
        var $this = this;

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
        function indexOfReplacementStart(syntaxes, str) {
            var syntax;
            var idx = -1;
            for (var i in syntaxes) {
                var j;
                // normal case - we search for the last occurrence to do a correct replacement
                // for nested variables, e.g. ${VAR1_${VAR2}}. This doesn't work in case
                // when start == end, e.g. @VAR@ - in that case we search from the start
                if (syntaxes[i].open !== syntaxes[i].close)
                    j = str.lastIndexOf(syntaxes[i].open);
                else
                    j = str.indexOf(syntaxes[i].open);
                if (j > idx) {
                    syntax = syntaxes[i];
                    idx = j;
                }
            }
            return { "syntax": syntax, "index": idx };
        }

        function expandString(key, str, env, seenVars) {
            if (!str)
                return str;
            var repl = indexOfReplacementStart(syntaxes, str);
            var i = repl.index;
            while (i !== -1) {
                var j = str.indexOf(repl.syntax.close, i + repl.syntax.open.length);
                if (j === -1)
                    return str;
                var varParts = str.slice(i + repl.syntax.open.length, j).split(':');
                var varName = varParts[0];
                var varFormatter = varParts[1];
                var envValue = env[varName];
                // if we end up expanding the same variable again, break the recursion
                if (seenVars.indexOf(varName) !== -1)
                    envValue = "";
                else
                    seenVars.push(varName);
                var varValue = expandString(key, envValue, env, seenVars);
                seenVars.pop();
                if (undefined === varValue) {
                    // skip replacement
                    if ($this.undefinedVariableFunction)
                        $this.undefinedVariableFunction(key, varName);
                    varValue = "";
                }
                varValue = String(varValue);
                if (varFormatter !== undefined) {
                    // TODO: XCode supports multiple formatters separated by a comma
                    var varFormatterLower = varFormatter.toLowerCase();
                    if (varFormatterLower === "rfc1034identifier" || varFormatterLower === "identifier")
                        varValue = Utilities.rfc1034Identifier(varValue);
                    if (varValue === "" && varFormatterLower.startsWith("default="))
                        varValue = varFormatter.split("=")[1];
                }
                str = str.slice(0, i) + varValue + str.slice(j + repl.syntax.close.length);
                repl = indexOfReplacementStart(syntaxes, str);
                i = repl.index;
            }
            return str;
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
                var expandedValue = expandString(key, value, env, []);
                if (expandedValue !== value)
                    obj[key] = expandedValue;
            }
        }
        expandRecursive(obj, env, []);
        return obj;
    };
    return PropertyListVariableExpander;
}());

/**
  * JSON.stringify(expandPlistEnvironmentVariables({a:"$(x)3$$(y)",b:{t:"%$(y) $(k)"}},
  *                                                {x:"X",y:"Y"}, true))
  *    Warning undefined variable  k  in variable expansion
  * => {"a":"X3$Y","b":{"t":"%Y $(k)"}}
  */
function expandPlistEnvironmentVariables(obj, env, warn) {
    var expander = new PropertyListVariableExpander();
    expander.undefinedVariableFunction = function (key, varName) {
        if (warn)
            console.warn("undefined variable " + varName + " in variable expansion");
    };
    return expander.expand(obj, env);
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
