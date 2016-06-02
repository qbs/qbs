/****************************************************************************
**
** Copyright (C) 2015 Jake Petroules.
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

var File = loadExtension("qbs.File");
var FileInfo = loadExtension("qbs.FileInfo");
var Process = loadExtension("qbs.Process");
var PropertyList = loadExtension("qbs.PropertyList");

function sdkInfoList(sdksPath) {
    var sdkInfo = [];
    var sdks = File.directoryEntries(sdksPath, File.Dirs | File.NoDotAndDotDot);
    for (var i in sdks) {
        // SDK directory name must contain a version number;
        // we don't want the versionless iPhoneOS.sdk directory for example
        if (!sdks[i].match(/[0-9]+/))
            continue;

        var settingsPlist = FileInfo.joinPaths(sdksPath, sdks[i], "SDKSettings.plist");
        var propertyList = new PropertyList();
        try {
            propertyList.readFromFile(settingsPlist);

            function checkPlist(plist) {
                if (!plist || !plist["CanonicalName"] || !plist["Version"])
                    return false;

                var re = /^([0-9]+)\.([0-9]+)$/;
                return plist["Version"].match(re);
            }

            var plist = propertyList.toObject();
            if (!checkPlist(plist)) {
                console.warn("Skipping corrupted SDK installation: "
                             + FileInfo.joinPaths(sdksPath, sdks[i]));
                continue;
            }

            sdkInfo.push(plist);
        } finally {
            propertyList.clear();
        }
    }

    // Sort by SDK version number
    sdkInfo.sort(function (a, b) {
        var re = /^([0-9]+)\.([0-9]+)$/;
        a = a["Version"].match(re);
        if (a)
            a = {major: a[1], minor: a[2]};
        b = b["Version"].match(re);
        if (b)
            b = {major: b[1], minor: b[2]};

        if (a.major === b.major)
            return a.minor - b.minor;
        return a.major - b.major;
    });

    return sdkInfo;
}

function findSigningIdentities(security, searchString) {
    var process;
    var identities;
    if (searchString) {
        try {
            process = new Process();
            if (process.exec(security, ["find-identity", "-p", "codesigning", "-v"], true) !== 0)
                console.error(process.readStdErr());

            var lines = process.readStdOut().split("\n");
            for (var i in lines) {
                // e.g. 1) AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "Mac Developer: John Doe (XXXXXXXXXX) john.doe@example.org"
                var match = lines[i].match(/^\s*[0-9]+\)\s+([A-Fa-f0-9]{40})\s+"([^"]+)"$/);
                if (match !== null) {
                    var hexId = match[1];
                    var displayName = match[2];
                    if (hexId === searchString || displayName.startsWith(searchString)) {
                        if (!identities)
                            identities = [];
                        identities.push([hexId, displayName]);
                        break;
                    }
                }
            }
        } finally {
            process.close();
        }
    }
    return identities;
}

function provisioningProfilePlistContents(filePath) {
    if (filePath === undefined)
        return undefined;

    var plist = new PropertyList();
    try {
        plist.readFromFile(filePath);
        return plist.toObject();
    } finally {
        plist.clear();
    }
}
