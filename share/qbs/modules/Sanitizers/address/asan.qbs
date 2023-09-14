/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

import qbs.Utilities

Module {
    Depends { name: "cpp" }

    property bool enabled: true
    readonly property bool _supported: qbs.toolchain.includes("gcc")
        || qbs.toolchain.includes("clang-cl")
        || (qbs.toolchain.includes("msvc")
            && Utilities.versionCompare(cpp.compilerVersion, "19.28.29500.0") >= 0)
    readonly property bool _enabled: enabled && _supported

    property string detectUseAfterReturn: "always"
    PropertyOptions {
        name: "detectUseAfterReturn"
        description: "Whether to detect problems with stack use after return from a function"
        allowedValues: ["never", "runtime", "always"]
    }

    property bool detectUseAfterScope: true

    cpp.driverFlags: {
        var flags = [];
        if (!_enabled)
            return flags;
        if (qbs.toolchain.includes("msvc") && !qbs.toolchain.includes("clang-cl")) {
            flags.push("/fsanitize=address");
            if (detectUseAfterReturn !== "never")
                flags.push("/fsanitize-address-use-after-return");
            return flags;
        }
        flags.push("-fsanitize=address", "-fno-omit-frame-pointer");
        if (detectUseAfterScope)
            flags.push("-fsanitize-address-use-after-scope");
        if (detectUseAfterReturn) {
            if (qbs.toolchain.includes("llvm")) {
                var minVersion = qbs.toolchain.contains("xcode") ? "14" : "13";
                if (Utilities.versionCompare(cpp.compilerVersion, minVersion) >= 0)
                    flags.push("-fsanitize-address-use-after-return=" + detectUseAfterReturn);
            } else if (detectUseAfterReturn === "never") {
                flags.push("--param", "asan-use-after-return=0");
            }
        }
        return flags;
    }
}
