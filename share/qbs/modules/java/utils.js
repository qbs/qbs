/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

var TextFile = loadExtension("qbs.TextFile");

// TODO: Parse exactly, so we won't get fooled by e.g. comments.
function extractPackageName(filePath)
{
    var file = new TextFile(filePath);
    var contents = file.readAll();
    file.close();
    var packageName = contents.replace(/[\s\S]*package\s+([^\s;]+)[\s;][\s\S]*/m, "$1");
    if (packageName === contents) // no package statement
        return "";
    return packageName;
}

function javacArguments(product, inputs) {
    var i;
    var outputDir = ModUtils.moduleProperty(product, "classFilesDir");
    var classPaths = [outputDir];
    var additionalClassPaths = ModUtils.moduleProperties(product, "additionalClassPaths");
    if (additionalClassPaths)
        classPaths = classPaths.concat(additionalClassPaths);
    for (i in inputs["java.jar"])
        classPaths.push(inputs["java.jar"][i].filePath);
    var debugArg = product.moduleProperty("qbs", "buildVariant") === "debug"
            ? "-g" : "-g:none";
    var args = [
            "-classpath", classPaths.join(product.moduleProperty("qbs", "pathListSeparator")),
            "-s", product.buildDirectory,
            debugArg, "-d", outputDir
        ];
    var runtimeVersion = ModUtils.moduleProperty(product, "runtimeVersion");
    if (runtimeVersion)
        args.push("-target", runtimeVersion);
    var languageVersion = ModUtils.moduleProperty(product, "languageVersion");
    if (languageVersion)
        args.push("-source", languageVersion);
    var bootClassPaths = ModUtils.moduleProperties(product, "bootClassPaths");
    if (bootClassPaths && bootClassPaths.length > 0)
        args.push("-bootclasspath", bootClassPaths.join(';'));
    if (!ModUtils.moduleProperty(product, "enableWarnings"))
        args.push("-nowarn");
    if (ModUtils.moduleProperty(product, "warningsAsErrors"))
        args.push("-Werror");
    var otherFlags = ModUtils.moduleProperty(product, "additionalCompilerFlags")
    if (otherFlags)
        args = args.concat(otherFlags);
    for (i = 0; i < inputs["java.java"].length; ++i)
        args.push(inputs["java.java"][i].filePath);
    return args;
}
