/****************************************************************************
**
** Copyright (C) 2024 Danya Patrushev <danyapat@yandex.ru>
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

var Process = require("qbs.Process");
var Host = require("qbs.Host");
var FileInfo = require("qbs.FileInfo");
var File = require("qbs.File");
var Environment = require("qbs.Environment");

function getEnvironment(shellPath, emccDir)
{
    var winOs = Host.os().includes("windows")
    var emsdkEnvFileName = "emsdk_env." + (winOs ? "bat" : "sh")
    var sep = FileInfo.pathSeparator()
    //if the compiler comes from emsdk its directory should be */emsdk/upstream/emscripten/
    var emsdkEnvFilePath = FileInfo.cleanPath(emccDir + sep + ".." + sep + ".." + sep + emsdkEnvFileName)

    if (!File.exists(emsdkEnvFilePath))
    {
        if (!winOs)
        {
            console.info("Emcc compiler found but " + emsdkEnvFilePath +
                     " doesn't exist - assuming compiler is provided by system package")
            return {}
        }
        else
            throw emsdkEnvFilePath + " not found";
    }
    else
    {
        var env = {}
        // If the environment already contains emsdk specific variables, running the script
        // will return a concise message about "Setting up EMSDK environment" without  all
        // the data that we need, so we check the environment first.

        var dir = Environment.getEnv("EMSDK")
        //use it as an indication that the environment is already set
        if (dir)
        {
            env["EMSDK"] = dir;
            var python = Environment.getEnv("EMSDK_PYTHON")
            if (python)
                env["EMSDK_PYTHON"] = python;
            var node = Environment.getEnv("EMSDK_NODE");
            if (node)
                env["EMSDK_NODE"] = node;
        }
        else
        {
            var process = new Process();
            process.exec(shellPath, [emsdkEnvFilePath]);

            var equalStr = " = ";
            var lines = process.readStdErr().split("\n");

            for (var i in lines)
            {
                var line = lines[i];
                var index = line.indexOf(equalStr);
                if (index === -1)
                    continue;

                var key = line.slice(0, index);
                if (key === "PATH")
                    continue;

                env[key] = line.slice(index + equalStr.length);
            }
        }

        return env;
    }
}
