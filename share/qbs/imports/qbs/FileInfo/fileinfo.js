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

var _windowsAbsolutePathPattern = new RegExp("^[a-z,A-Z]:[/,\\\\]");
var _removeDoubleSlashesPattern = new RegExp("/{2,}", "g");

function path(fp) {
    if (fp === '/')
        return fp;

    // Yes, this will be wrong for "clever" unix users calling their directory 'c:'. Boohoo.
    if (fp.length === 3 && fp.slice(-2) === ":/")
        return fp;

    var last = fp.lastIndexOf('/');
    if (last < 0)
        return '.';
    return fp.slice(0, last);
}

function fileName(fph) {
    var fp = fph.toString();
    var last = fp.lastIndexOf('/');
    if (last < 0)
        return fp;
    return fp.slice(last + 1);
}

function baseName(fph) {
    var fn = fileName(fph);
    return fn.split('.')[0];
}

function completeBaseName(fph) {
    var fn = fileName(fph);
    var last = fn.lastIndexOf('.');
    if (last < 0)
        return fn;
    else
        return fn.slice(0, last);
}

function relativePath(base, rel) {
    var basel = base.split('/');
    var rell = rel.split('/');
    var i;
    for (i = basel.length; i-- >= 0;) {
        if (basel[i] === '.' || basel[i] === '')
            basel.splice(i, 1);
    }
    for (i = rell.length; i-- >= 0;) {
        if (rell[i] === '.' || rell[i] === '')
            rell.splice(i, 1);
    }

    i = 0;
    while (i < basel.length && i < rell.length && basel[i] === rell[i])
        i++;

    var j = i;
    var r = [];

    for (; i < basel.length; i++)
        r.push("..");

    for (; j < rell.length; j++)
        r.push(rell[j]);

    return r.join('/');
}

function isAbsolutePath(path) {
    if (!path)
        return false;
    return (path.charAt(0) === '/' || _windowsAbsolutePathPattern.test(path));
}

function toWindowsSeparators(str) {
    return str.toString().replace(/\//g, '\\');
}

function fromWindowsSeparators(str) {
    return str.toString().replace(/\\/g, '/');
}

function toNativeSeparators(str, os) {
    return os.contains("windows") ? toWindowsSeparators(str) : str;
}

function fromNativeSeparators(str, os) {
    return os.contains("windows") ? fromWindowsSeparators(str) : str;
}

function joinPaths() {
    function pathFilter(path) {
        return path && typeof path === "string";
    }

    var paths = Array.prototype.slice.call(arguments, 0).filter(pathFilter);
    var joinedPaths = paths.join('/');

    return joinedPaths.replace(_removeDoubleSlashesPattern, '/');
}
