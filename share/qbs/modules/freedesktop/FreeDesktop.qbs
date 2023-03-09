/****************************************************************************
**
** Copyright (C) 2019 Alberto Mardegan <mardy@users.sourceforge.net>
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

import qbs.ModUtils
import qbs.TextFile
import "freedesktop.js" as Fdo

Module {
    property string name: product.name
    property string appName: name

    property var desktopKeys

    readonly property var defaultDesktopKeys: {
        return {
            'Type': 'Application',
            'Name': product.freedesktop.appName,
            'Exec': product.targetName,
            'Terminal': 'false',
            'Version': '1.1',
        }
    }
    property bool _fdoSupported: qbs.targetOS.includes("unix") && !qbs.targetOS.includes("darwin")

    additionalProductTypes: "freedesktop.desktopfile"

    FileTagger {
        patterns: [ "*.desktop" ]
        fileTags: [ "freedesktop.desktopfile_source" ]
    }

    Rule {
        condition: _fdoSupported

        inputs: [ "freedesktop.desktopfile_source" ]
        outputFileTags: [ "freedesktop.desktopfile" ]

        Artifact {
            fileTags: [ "freedesktop.desktopfile" ]
            filePath: input.fileName
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName + " from " + input.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var aggregateDesktopKeys = Fdo.parseDesktopFile(input.filePath);
                var desktopKeys = ModUtils.moduleProperty(product, "desktopKeys") || {}
                var mainSection = aggregateDesktopKeys['Desktop Entry'];
                for (key in desktopKeys) {
                    if (desktopKeys.hasOwnProperty(key)) {
                        mainSection[key] = desktopKeys[key];
                    }
                }

                var defaultValues = product.freedesktop.defaultDesktopKeys
                for (key in defaultValues) {
                    if (!(key in mainSection)) {
                        mainSection[key] = defaultValues[key];
                    }
                }

                Fdo.dumpDesktopFile(output.filePath, aggregateDesktopKeys);
            }
            return [cmd];
        }
    }

    Group {
        condition: product.freedesktop._fdoSupported
        fileTagsFilter: [ "freedesktop.desktopfile" ]
        qbs.install: true
        qbs.installDir: "share/applications"
    }

    Group {
        condition: product.freedesktop._fdoSupported
        fileTagsFilter: [ "freedesktop.appIcon" ]
        qbs.install: true
        qbs.installDir: "share/icons/hicolor/scalable/apps"
    }

    FileTagger {
        patterns: [ "*.metainfo.xml", "*.appdata.xml" ]
        fileTags: [ "freedesktop.appstream" ]
    }

    Group {
        condition: product.freedesktop._fdoSupported
        fileTagsFilter: [ "freedesktop.appstream" ]
        qbs.install: true
        qbs.installDir: "share/metainfo"
    }
}
