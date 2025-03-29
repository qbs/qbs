/****************************************************************************
**
** Copyright (C) 2025 Ivan Komissarov (abbapoh@gmail.com).
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

var FileInfo = require('qbs.FileInfo');

function getOutputFileTags(input) {
    const tags = ["qt.compiled_shader"];
    if (input.fileTags.includes("qt.shader.vertex")) {
        tags.push("qt.compiled_shader.vertex");
    }
    if (input.fileTags.includes("qt.shader.tessellation")) {
        tags.push("qt.compiled_shader.tessellation");
    }
    if (input.fileTags.includes("qt.shader.fragment")) {
        tags.push("qt.compiled_shader.fragment");
    }
    if (input.fileTags.includes("qt.shader.compute")) {
        tags.push("qt.compiled_shader.compute");
    }
    return tags;
}

function compilerOutputArtifacts(input) {
    var artifacts = [];
    const addResourceData = input.Qt.shadertools.addResourceData;
    artifacts.push({
        filePath: FileInfo.joinPaths(input.Qt.shadertools.generatedShadersDir,
                                     input.fileName + '.qsb'),
        fileTags: getOutputFileTags(input).concat(addResourceData ? ["qt.core.resource_data"] : []),
        Qt: addResourceData ? {
            core: {
                resourcePrefix: input.Qt.core.resourcePrefix
            }
        } : {}
    });
    return artifacts;
}

function compilerCommands(project, product, inputs, outputs, input, output) {
    const qsbPath = FileInfo.joinPaths(input.Qt.core.binPath, input.Qt.shadertools.qsbName);
    var glslVersions = input.Qt.shadertools.glslVersions;
    var hlslVersions = input.Qt.shadertools.hlslVersions;
    var mslVersions = input.Qt.shadertools.mslVersions;

    var args = [input.filePath, '-o', output.filePath];

    if (input.Qt.shadertools.useQt6Versions
        && glslVersions.length === 0 && hlslVersions.length === 0 && mslVersions.length === 0) {
        glslVersions = ['100 es', '120', '150'];
        hlslVersions = ['50'];
        mslVersions = ['20'];
    }

    if (glslVersions.length > 0) {
        args.push('--glsl', glslVersions.join(','));
    }
    if (hlslVersions.length > 0) {
        args.push('--hlsl', hlslVersions.join(','));
    }
    if (mslVersions.length > 0) {
        args.push('--msl', mslVersions.join(','));
    }
    if (input.Qt.shadertools.batchable) {
        args.push('-b');
        if (input.Qt.shadertools.zOrderLocation !== undefined) {
            args.push('--zorder-loc', input.Qt.shadertools.zOrderLocation);
        }
    }
    if (input.Qt.shadertools.tessellation && input.fileTags.includes("qt.shader.tessellation")) {
        args.push('--msltess');
        const tessellationMode = input.Qt.shadertools.tessellationMode;
        if (tessellationMode !== undefined) {
            args.push('--tess-mode', tessellationMode);
        }
        const tessellationVertexCount = input.Qt.shadertools.tessellationVertexCount;
        if (tessellationVertexCount !== undefined) {
            args.push('--tess-vertex-count', tessellationVertexCount);
        }
    }

    const viewCount = input.Qt.shadertools.viewCount;
    if (viewCount !== undefined) {
        args.push('--view-count', viewCount);
    }

    if (input.Qt.shadertools.optimized) {
        args.push('-O');
    }

    const defines = input.Qt.shadertools.defines || [];
    if (defines.length > 0) {
        args = args.concat(defines.map(function(define) {
            return '-D' + define;
        }));
    }

    if (input.Qt.shadertools.debugInformation) {
        args.push('-g');
    }

    var cmd = new Command(qsbPath, args);
    cmd.description = 'compiling ' + output.fileName;
    cmd.highlight = 'codegen';
    cmd.environment = ["LANG=en_US.UTF-8"];
    return [cmd];
}
