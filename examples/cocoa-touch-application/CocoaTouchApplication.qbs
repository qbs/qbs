/****************************************************************************
**
** Copyright (C) 2015 Petroules Corporation.
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import qbs 1.0

CppApplication {
    Depends { condition: product.condition; name: "ib" }
    condition: qbs.hostOS.contains("macos")
    name: "Cocoa Touch Application"

    qbs.targetPlatform: "ios"

    cpp.useObjcPrecompiledHeader: true
    cpp.minimumIosVersion: "8.0"
    cpp.frameworks: [ "UIKit", "Foundation", "CoreGraphics" ]

    Group {
        prefix: "CocoaTouchApplication/"
        files: [
            "AppDelegate.h",
            "AppDelegate.m",
            "CocoaTouchApplication-Info.plist",
            "Default-568h@2x.png",
            "Default.png",
            "Default@2x.png",
            "DetailViewController.h",
            "DetailViewController.m",
            "MasterViewController.h",
            "MasterViewController.m",
            "main.m"
        ]
    }

    Group {
        name: "Supporting Files"
        prefix: "CocoaTouchApplication/en.lproj/"
        files: [
            "DetailViewController_iPad.xib",
            "DetailViewController_iPhone.xib",
            "InfoPlist.strings",
            "MasterViewController_iPad.xib",
            "MasterViewController_iPhone.xib"
        ]
    }

    Group {
        name: "Xcode Project"
        files: [
            "CocoaTouchApplication.xcodeproj/project.pbxproj"
        ]
    }

    Group {
        files: ["CocoaTouchApplication/CocoaTouchApplication-Prefix.pch"]
        fileTags: ["objc_pch_src"]
    }
}
