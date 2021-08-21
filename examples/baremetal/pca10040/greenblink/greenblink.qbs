/****************************************************************************
**
** Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
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
        if (!qbs.architecture.startsWith("arm"))
            return false;
        return (qbs.toolchain.contains("gcc")
                || qbs.toolchain.contains("iar")
                || qbs.toolchain.contains("keil"))
                && !qbs.toolchain.contains("xcode")
    }
    name: "pca10040-greenblink"
    cpp.cLanguageVersion: "c99"
    cpp.positionIndependentCode: false

    //
    // GCC-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("gcc")
        cpp.assemblerFlags: [
            "-mcpu=cortex-m4",
            "-mfloat-abi=hard",
            "-mfpu=fpv4-sp-d16",
        ]
        cpp.driverFlags: [
            "-mcpu=cortex-m4",
            "-mfloat-abi=hard",
            "-mfpu=fpv4-sp-d16",
            "-specs=nosys.specs"
        ]
    }

    Group {
        condition: qbs.toolchain.contains("gcc")
        name: "GCC"
        prefix: "gcc/"
        Group {
            name: "Startup"
            fileTags: ["asm"]
            files: ["startup.s"]
        }
        Group {
            name: "Linker Script"
            fileTags: ["linkerscript"]
            files: ["flash.ld"]
        }
    }

    //
    // IAR-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("iar")
        cpp.assemblerFlags: [
            "--cpu", "cortex-m4",
            "--fpu", "vfpv4_sp"
        ]
        cpp.driverFlags: [
            "--cpu", "cortex-m4",
            "--fpu", "vfpv4_sp"
        ]
    }

    Group {
        condition: qbs.toolchain.contains("iar")
        name: "IAR"
        prefix: "iar/"
        Group {
            name: "Startup"
            fileTags: ["asm"]
            files: ["startup.s"]
        }
        Group {
            name: "Linker Script"
            fileTags: ["linkerscript"]
            files: ["flash.icf"]
        }
    }

    //
    // KEIL-specific properties and sources.
    //

    Properties {
        condition: qbs.toolchain.contains("keil")
        cpp.assemblerFlags: [
            "--cpu", "cortex-m4.fp.sp"
        ]
        cpp.driverFlags: [
            "--cpu", "cortex-m4.fp.sp"
        ]
    }

    Group {
        condition: qbs.toolchain.contains("keil")
        name: "KEIL"
        prefix: "keil/"
        Group {
            name: "Startup"
            fileTags: ["asm"]
            files: ["startup.s"]
        }
        Group {
            name: "Linker Script"
            fileTags: ["linkerscript"]
            files: ["flash.sct"]
        }
    }

    //
    // Common code.
    //

    Group {
        name: "Gpio"
        files: ["gpio.c", "gpio.h"]
    }

    Group {
        name: "System"
        files: ["system.h"]
    }

    files: ["main.c"]
}
