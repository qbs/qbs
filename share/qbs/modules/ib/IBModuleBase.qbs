/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com)
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

Module {
    priority: -100

    property bool warnings
    property bool errors
    property bool notices

    property stringList flags

    // tiffutil specific
    property string tiffutilName
    property string tiffutilPath
    property bool combineHidpiImages

    // iconutil specific
    property string iconutilName
    property string iconutilPath

    // XIB/NIB specific
    property string ibtoolName
    property string ibtoolPath
    property bool flatten
    property string module
    property bool autoActivateCustomFonts

    // Asset catalog specific
    property string actoolName
    property string actoolPath
    property string appIconName
    property string launchImageName
    property bool compressPngs

    // private properties
    property string outputFormat
    property string tiffSuffix
    property string appleIconSuffix
    property string compiledAssetCatalogSuffix
    property string compiledNibSuffix
    property string compiledStoryboardSuffix

    property string ibtoolVersion
    property var ibtoolVersionParts
    property int ibtoolVersionMajor
    property int ibtoolVersionMinor
    property int ibtoolVersionPatch

    property stringList targetDevices
}
