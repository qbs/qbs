import "../multiarch-helpers.js" as Helpers

Project {
    readonly property bool enableCodeSigning: !qbs.targetOS.includes("ios")
        || qbs.targetOS.includes("ios-simulator")

    property bool multiplexArchitectures: false
    property bool multiplexBuildVariants: false
    property string xcodeVersion

    Product {
        Depends { name: "bundle" }
        condition: {
            console.info("bundle.isShallow: " + bundle.isShallow);
            console.info("qbs.targetOS: " + qbs.targetOS);
            console.info("enableCodeSigning: " + project.enableCodeSigning);
            return false;
        }
    }

    // main bundle
    Application {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        Depends { name: "A" } // framework
        Depends { name: "B" } // static framework
        Depends { name: "C" } // loadable module
        Depends { name: "D" } // dynamic library
        Depends { name: "E" } // static library
        Depends { name: "F" } // application
        Depends { name: "G" } // executable
        Depends { name: "H"; bundle.isForMainBundle: false; } // executable, not copied
        name: "MainApp"
        bundle.isBundle: true
        bundle.isMainBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "A"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    StaticLibrary {
        Depends { name: "cpp" }
        name: "B"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        bundle.isForMainBundle: true // static library is not copied by default, override
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    LoadableModule {
        Depends { name: "cpp" }
        name: "C"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        // qbs.buildVariants: project.isMultiplexed ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "D"
        bundle.isBundle: false
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    StaticLibrary {
        Depends { name: "cpp" }
        name: "E"
        bundle.isBundle: false
        bundle.isForMainBundle: true
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    // bundle application
    Application {
        Depends { name: "cpp" }
        name: "F"
        bundle.isBundle: true
        bundle.resources: ["resource.txt"]
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    // non-bundle application
    Application {
        Depends { name: "cpp" }
        name: "G"
        bundle.isBundle: false
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }

    // non-bundle application
    Application {
        Depends { name: "cpp" }
        name: "H"
        bundle.isBundle: false
        codesign.enableCodeSigning: project.enableCodeSigning
        qbs.architectures: project.multiplexArchitectures
            ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiplexBuildVariants ? ["debug", "release"] : []
        files: ["dummy.c"]
        install: false
        installDir: ""
    }
}
