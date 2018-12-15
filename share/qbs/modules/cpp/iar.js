/****************************************************************************
**
** Copyright (C) 2018 Denis Shienkov <denis.shienkov@gmail.com>
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

var Cpp = require("cpp.js");
var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var Process = require("qbs.Process");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");
var WindowsUtils = require("qbs.WindowsUtils");

function guessArchitecture(macros)
{
    if (macros['__ICCARM__'] === '1')
        return 'arm';
}

function guessEndianness(macros)
{
    if (macros['__LITTLE_ENDIAN__'] === '1')
        return 'little';
}

function dumpMacros(compilerFilePath, qbs, nullDevice) {
    var fn = FileInfo.fromNativeSeparators(Environment.getEnv('TEMP') + '/iar-macros.predef');
    var p = new Process();
    try {
        p.exec(compilerFilePath,
               [ nullDevice, '--predef_macros', fn ],
               true);
        try {
            var tf = new TextFile(fn, TextFile.ReadOnly);
            var map = {};
            tf.readAll().trim().split(/\r?\n/g).map(function (line) {
                var parts = line.split(" ", 3);
                map[parts[1]] = parts[2];
            });
            return map;
        } finally {
            if (tf)
                tf.close();
        }
    } finally {
        p.close();
    }
}

function targetCores(input) {
    var cores = [];
    var cpu = input.cpp.cpu;
    if (cpu) {
        cores.push('--cpu');
        cores.push(cpu);
    }
    var fpu = input.cpp.fpu;
    if (fpu) {
        cores.push('--fpu');
        cores.push(fpu);
    }
    return cores;
}

function compilerFlags(project, product, input, output, explicitlyDependsOn) {
    // Determine which C-language we're compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!['c', 'cpp'].contains(tag))
        throw ('unsupported source language');

    var i;
    var args = [];
    args.push(input.filePath);
    args = args.concat(targetCores(input));

    switch (input.cpp.optimization) {
    case "small":
        args.push('-Ohs');
        break;
    case "fast":
        args.push('-Ohz');
        break;
    case "none":
        args.push("-On");
        break;
    }

    // Enable IAR C/C++ language extensions.
    args.push('-e');

    if (input.cpp.debugInformation) {
        args.push('--debug');
        // Disable static clustering for static and global variables.
        args.push('--no_clustering');
        // Disable code motion.
        args.push('--no_code_motion');
        // Disable common sub-expression elimination.
        args.push('--no_cse');
        // Disable function inlining.
        args.push('--no_inline');
        // Disable instruction scheduling.
        args.push('--no_scheduling');
        // Disable type based alias analysis.
        args.push('--no_tbaa');
        // Disable loop unrolling.
        args.push('--no_unroll');
    }

    var warnings = input.cpp.warningLevel;
    if (warnings === 'none') {
        args.push('--no_warnings');
    } else if (warnings === 'all') {
        args.push('--warnings_affect_exit_code');
        args.push('--warn_about_c_style_casts');
    }
    if (input.cpp.treatWarningsAsErrors) {
        args.push('--warnings_are_errors');
    }

    // Choose byte order.
    var endianness = input.cpp.endianness;
    if (endianness)
        args.push('--endian=' + endianness);

    // Specify DLib library configuration.
    var dlibPath = input.cpp.toolchainInstallPath + "/../inc/c/DLib_Config_Full.h";
    args.push('--dlib_config', dlibPath);

    if (tag === 'c') {
        // Language version.
        if (input.cpp.cLanguageVersion === 'c89')
            args.push('--c89');
    } else if (tag === 'cpp') {
        args.push('--c++');
        if (!input.cpp.enableExceptions)
            args.push('--no_exceptions');
        if (!input.cpp.enableRtti)
            args.push('--no_rtti');
        if (!input.cpp.enableStaticDestruction)
            args.push('--no_static_destruction');
    }

    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    args = args.concat(allDefines.map(function(define) { return '-D' + define }));

    var allIncludePaths = [];
    if (input.cpp.enableCmsis) {
        var cmsisIncludePath = input.cpp.toolchainInstallPath + "/../CMSIS/Include/";
        allIncludePaths.push(cmsisIncludePath);
    }
    var includePaths = input.cpp.includePaths;
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var compilerIncludePaths = input.cpp.compilerIncludePaths;
    if (compilerIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(compilerIncludePaths);
    args = args.concat(allIncludePaths.map(function(include) { return '-I' + include }));

    args.push('-o', output.filePath);

    args = args.concat(ModUtils.moduleProperty(input, 'platformFlags'),
                       ModUtils.moduleProperty(input, 'flags'),
                       ModUtils.moduleProperty(input, 'platformFlags', tag),
                       ModUtils.moduleProperty(input, 'flags', tag));
    return args;
}

function assemblerFlags(project, product, input, output, explicitlyDependsOn) {
    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!["asm"].contains(tag))
        throw ("unsupported assembler language");

    var args = [];
    args.push(input.filePath);
    args = args.concat(targetCores(input));

    if (input.cpp.debugInformation)
        args.push('-r');

    var warnings = input.cpp.warningLevel;
    if (warnings === 'none')
        args.push('-w-');
    else
        args.push('-w+');

    // Enable sensitivity for user symbols.
    args.push('-s+');

    // ASM macro argument quotes.
    var quotes = input.cpp.macroQuoteCharacters;
    if (quotes)
        args.push('-M' + quotes);

    args.push('-o', output.filePath);

    args = args.concat(ModUtils.moduleProperty(input, 'platformFlags', tag),
                       ModUtils.moduleProperty(input, 'flags', tag));
    return args;
}

function linkerFlags(project, product, input, output) {
    var i;
    var args = [];

    if (inputs.obj)
        args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

    var redirects = [ '_Printf=_PrintfFull', '_Scanf=_ScanfFull' ];
    for (i in redirects) {
        args.push('--redirect');
        args.push(redirects[i]);
    }

    args.push('-o', output.filePath);

    var linkerScripts = inputs.linkerscript
            ? inputs.linkerscript.map(function(a) { return a.filePath; }) : [];
    for (i in linkerScripts) {
        args.push('--config');
        args.push(linkerScripts[i]);
    }

    if (product.cpp.debugInformation)
        args.push('--semihosting');

    // Program entry point.
    var entryPoint = product.cpp.entryPoint
            ? product.cpp.entryPoint : '__iar_program_start'
    args.push('--entry');
    args.push(entryPoint);

    // Perform virtual function elimination.
    args.push('--vfe');

    return args;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, output, explicitlyDependsOn);
    var compilerPath = product.cpp.compilerPath;
    var cmd = new Command(compilerPath, args)
    cmd.description = 'compiling ' + input.fileName;
    cmd.highlight = "compiler";
    cmd.workingDirectory = product.buildDirectory;
    cmd.responseFileUsagePrefix = '@';
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, output, explicitlyDependsOn);
    var assemblerPath = product.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args)
    cmd.description = 'assembling ' + input.fileName;
    cmd.highlight = "compiler";
    cmd.workingDirectory = product.buildDirectory;
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, input, output);
    var linkerPath = product.cpp.linkerPath;
    var cmd = new Command(linkerPath, args)
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.workingDirectory = product.buildDirectory;
    return [cmd];
}
