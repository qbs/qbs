/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

var ModUtils = loadExtension("qbs.ModUtils");

function args(product, input, outputFileName)
{
    var defines = product.moduleProperty("cpp", "compilerDefines");
    defines = defines.uniqueConcat(product.moduleProperty("cpp", "platformDefines"));
    defines = defines.uniqueConcat(
                ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'defines'));
    var includePaths = ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'includePaths');
    includePaths = includePaths.uniqueConcat(ModUtils.modulePropertiesFromArtifacts(
                                                 product, [input], 'cpp', 'systemIncludePaths'));
    var useCompilerPaths = product.moduleProperty("Qt.core", "versionMajor") >= 5;
    if (useCompilerPaths) {
        includePaths = includePaths.uniqueConcat(ModUtils.modulePropertiesFromArtifacts(
                                                 product, [input], 'cpp', 'compilerIncludePaths'));
    }
    var frameworkPaths = product.moduleProperty("cpp", "frameworkPaths");
    frameworkPaths = frameworkPaths.uniqueConcat(
                product.moduleProperty("cpp", "systemFrameworkPaths"));
    if (useCompilerPaths) {
        frameworkPaths = frameworkPaths.uniqueConcat(
                    product.moduleProperty("cpp", "compilerFrameworkPaths"));
    }
    var pluginMetaData = product.moduleProperty("Qt.core", "pluginMetaData");
    var args = [];
    args = args.concat(
                defines.map(function(item) { return '-D' + item; }),
                includePaths.map(function(item) { return '-I' + item; }),
                frameworkPaths.map(function(item) { return '-F' + item; }),
                pluginMetaData.map(function(item) { return '-M' + item; }),
                ModUtils.moduleProperty(product, "mocFlags"),
                '-o', outputFileName,
                input.filePath);
    return args;
}

function fullPath(product)
{
    return ModUtils.moduleProperty(product, "binPath") + '/' + ModUtils.moduleProperty(product, "mocName");
}
