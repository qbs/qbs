import "shadertools.js" as ShaderTools

QtModule {
    qtModuleName: "ShaderTools"

    additionalProductTypes: ["qt.compiled_shader"]

    property string generatedShadersDir: "shaders"
    property string qsbName: "qsb"
    property stringList glslVersions
    property stringList hlslVersions
    property stringList mslVersions
    property bool useQt6Versions: true
    property bool tessellation: true
    property string tessellationMode
    PropertyOptions {
        name: "tessellationMode"
        allowedValues: ["triangles", "quads"]
    }
    property int tessellationVertexCount
    property int viewCount
    property bool batchable: false
    property int zOrderLocation

    property bool optimized: false

    property stringList defines
    property bool debugInformation: false

    property bool addResourceData: true
    property bool useCompiler: !enableLinking

    Depends { name: "Qt"; submodules: @dependencies@ }

    version: @version@
    hasLibrary: @has_library@
    architectures: @archs@
    targetPlatform: @targetPlatform@
    staticLibsDebug: @staticLibsDebug@
    staticLibsRelease: @staticLibsRelease@
    dynamicLibsDebug: @dynamicLibsDebug@
    dynamicLibsRelease: @dynamicLibsRelease@
    linkerFlagsDebug: @linkerFlagsDebug@
    linkerFlagsRelease: @linkerFlagsRelease@
    frameworksDebug: @frameworksDebug@
    frameworksRelease: @frameworksRelease@
    frameworkPathsDebug: @frameworkPathsDebug@
    frameworkPathsRelease: @frameworkPathsRelease@
    libNameForLinkerDebug: @libNameForLinkerDebug@
    libNameForLinkerRelease: @libNameForLinkerRelease@
    libFilePathDebug: @libFilePathDebug@
    libFilePathRelease: @libFilePathRelease@
    pluginTypes: @pluginTypes@
    moduleConfig: @moduleConfig@
    cpp.defines: @defines@
    cpp.systemIncludePaths: @includes@
    cpp.libraryPaths: @libraryPaths@
    @additionalContent@

    enableLinking: false

    Group {
        condition: useCompiler

        FileTagger {
            patterns: ["*.vert"]
            fileTags: ["qt.shader.vertex", "qt.shader"]
        }

        FileTagger {
            patterns: ["*.tesc", "*.tese"]
            fileTags: ["qt.shader.tessellation", "qt.shader"]
        }

        FileTagger {
            patterns: ["*.frag"]
            fileTags: ["qt.shader.fragment", "qt.shader"]
        }

        FileTagger {
            patterns: ["*.comp"]
            fileTags: ["qt.shader.compute", "qt.shader"]
        }

        Rule {
            inputs: ["qt.shader"]
            outputFileTags: [
                "qt.compiled_shader",
                "qt.compiled_shader.vertex",
                "qt.compiled_shader.tessellation",
                "qt.compiled_shader.fragment",
                "qt.compiled_shader.compute",
                "qt.core.resource_data",
            ]
            outputArtifacts: ShaderTools.compilerOutputArtifacts(input)
            prepare: ShaderTools.compilerCommands.apply(ShaderTools, arguments)
        }
    }
}
