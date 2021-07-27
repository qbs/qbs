/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

CppApplication {
    condition: {
        if (!qbs.architecture.contains("msp430"))
            return false;
        return qbs.toolchain.contains("iar")
                || qbs.toolchain.contains("gcc")
    }
    name: "msp430f5529-nes-gamepads"
    cpp.cLanguageVersion: "c99"
    cpp.positionIndependentCode: false

    //
    // IAR-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("iar")

        property path dlibIncludePath: cpp.toolchainInstallPath + "/../lib/dlib/dl430xssfn.h"
        property path dlibRuntimePath: cpp.toolchainInstallPath + "/../lib/dlib/dl430xssfn.r43"

        cpp.entryPoint: "__program_start"

        cpp.defines: ["__MSP430F5529__"]

        cpp.driverFlags: [
            "-e",
            "--core=430X",
            "--data_model=small",
            "--code_model=small",
            "--dlib_config", dlibIncludePath
        ]

        cpp.driverLinkerFlags: [
            "-D_STACK_SIZE=A0",
            "-D_DATA16_HEAP_SIZE=A0",
            "-D_DATA20_HEAP_SIZE=50"
        ]

        cpp.staticLibraries: [
            // Explicitly link with the runtime dlib library (which contains
            // all required startup code and other stuff).
            dlibRuntimePath
        ]
    }

    Group {
        condition: qbs.toolchain.contains("iar")
        name: "IAR Linker Script"
        prefix: cpp.toolchainInstallPath + "/../config/linker/"
        fileTags: ["linkerscript"]
        // Explicitly use the default linker scripts for current target.
        files: ["lnk430f5529.xcl"]
    }

    //
    // GCC-specific properties and soucres.
    //

    Properties {
        condition: qbs.toolchain.contains("gcc")
        property path supportFilesPath
        // A path to the MSP430 support files, which are
        // provided by the Texas Instruments separately:
        // e.g. 'c:/msp430-gcc-support-files/include/'
        cpp.includePaths: supportFilesPath
        cpp.libraryPaths: supportFilesPath
        cpp.driverFlags: ["-mmcu=msp430f5529"]
    }

    Group {
        condition: qbs.toolchain.contains("gcc")
        name: "GCC Linker Script"
        fileTags: ["linkerscript"]
        files: ["gamepads.ld"]
    }

    files: [
        "gpio.c",
        "gpio.h",
        "hid.h",
        "hiddesc.c",
        "hidep0.c",
        "hidep1.c",
        "hwdefs.h",
        "main.c",
        "pmm.c",
        "pmm.h",
        "ucs.c",
        "ucs.h",
        "usb.c",
        "usb.h",
        "wdt_a.c",
        "wdt_a.h",
    ]
}
