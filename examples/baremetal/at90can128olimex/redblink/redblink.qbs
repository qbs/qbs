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
        if (!qbs.architecture.contains("avr"))
            return false;
        return qbs.toolchain.contains("gcc")
            || qbs.toolchain.contains("iar")
    }
    name: "at90can128olimex-redblink"
    cpp.cLanguageVersion: "c99"
    cpp.positionIndependentCode: false

    //
    // GCC-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("gcc")
        cpp.driverFlags: ["-mmcu=at90can128"]
    }

    // Note: We implicitly use the startup file and the linker script
    // which are already linked into the avr-gcc toolchain runtime.

    //
    // IAR-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("iar")
        cpp.driverFlags: ["--cpu=can128", "-ms"]
        cpp.entryPoint: "__program_start"
        cpp.driverLinkerFlags: [
            "-D_..X_HEAP_SIZE=0",
            "-D_..X_NEAR_HEAP_SIZE=20",
            "-D_..X_CSTACK_SIZE=20",
            "-D_..X_RSTACK_SIZE=20",
            "-D_..X_FLASH_CODE_END=1FFFF",
            "-D_..X_FLASH_BASE=_..X_INTVEC_SIZE",
            "-D_..X_EXT_SRAM_BASE=_..X_SRAM_END",
            "-D_..X_EXT_ROM_BASE=_..X_SRAM_END",
            "-D_..X_EXT_NV_BASE=_..X_SRAM_END",
            "-D_..X_CSTACK_BASE=_..X_SRAM_BASE",
            "-D_..X_CSTACK_END=_..X_SRAM_END",
            "-D_..X_RSTACK_BASE=_..X_SRAM_BASE",
            "-D_..X_RSTACK_END=_..X_SRAM_END",
            "-h'1895'(CODE)0-(_..X_INTVEC_SIZE-1)",
        ]
        cpp.staticLibraries: [
            // Explicitly link with the runtime dlib library (which contains
            // all required startup code and other stuff).
            cpp.toolchainInstallPath + "/../lib/dlib/dlAVR-3s-ec_mul-n.r90"
        ]
    }

    Group {
        condition: qbs.toolchain.contains("iar")
        name: "IAR"
        prefix: "iar/"
        Group {
            name: "Linker Script"
            prefix: cpp.toolchainInstallPath + "/../src/template/"
            fileTags: ["linkerscript"]
            // Explicitly use the default linker scripts for current target.
            files: ["cfg3soim.xcl", "cfgcan128.xcl"]
        }
    }

    //
    // Common code.
    //

    Group {
        name: "Gpio"
        files: ["gpio.c", "gpio.h"]
    }

    files: ["main.c"]
}
