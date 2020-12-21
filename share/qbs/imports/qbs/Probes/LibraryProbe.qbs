/****************************************************************************
**
** Copyright (C) 2018 Ivan Komissarov (abbapoh@gmail.com)
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

PathProbe {
    property string endianness
    nameSuffixes: {
        if (qbs.targetOS.contains("windows"))
            return [".lib"];
        if (qbs.targetOS.contains("macos"))
            return [".dylib", ".a"];
        return [".so", ".a"];
    }
    platformSearchPaths: {
        var result = [];
        if (qbs.targetOS.contains("unix")) {
            if (qbs.targetOS.contains("linux") && qbs.architecture) {
                if (qbs.architecture === "armv7")
                    result = ["/usr/lib/arm-linux-gnueabihf"]
                else if (qbs.architecture === "arm64")
                    result = ["/usr/lib/aarch64-linux-gnu"]
                else if (qbs.architecture === "mips" && endianness === "big")
                    result = ["/usr/lib/mips-linux-gnu"]
                else if (qbs.architecture === "mips" && endianness === "little")
                    result = ["/usr/lib/mipsel-linux-gnu"]
                else if (qbs.architecture === "mips64")
                    result = ["/usr/lib/mips64el-linux-gnuabi64"]
                else if (qbs.architecture === "ppc")
                    result = ["/usr/lib/powerpc-linux-gnu"]
                else if (qbs.architecture === "ppc64")
                    result = ["/usr/lib/powerpc64le-linux-gnu"]
                else if (qbs.architecture === "x86_64")
                    result = ["/usr/lib64", "/usr/lib/x86_64-linux-gnu"]
                else if (qbs.architecture === "x86")
                    result = ["/usr/lib32", "/usr/lib/i386-linux-gnu"]
            }
            result = result.concat(["/usr/lib", "/usr/local/lib", "/lib", "/app/lib"]);
        }

        return result;
    }
    nameFilter: {
        if (qbs.targetOS.contains("unix")) {
            return function(name) {
                return "lib" + name;
            }
        } else {
            return function(name) {
                return name;
            }
        }
    }
    platformEnvironmentPaths: {
        if (qbs.targetOS.contains("windows"))
            return [ "PATH" ];
        else
            return [ "LIBRARY_PATH" ];
    }
}
