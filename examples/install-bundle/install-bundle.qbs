import qbs
import qbs.FileInfo

Project {
    CppApplication {
        Depends { name: "coreutils" }
        Depends { name: "ib"; condition: qbs.targetOS.contains("darwin") }
        Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

        name: "window"
        targetName: bundle.isBundle ? "Window" : "window"
        files: [
            "main.cpp",
            "assetcatalog1.xcassets",
            "assetcatalog2.xcassets",
            "white.iconset",
            "MainMenu.xib",
            "Storyboard.storyboard"
        ]

        Group {
            fileTagsFilter: bundle.isBundle ? ["bundle.content"] : ["application"]
            qbs.install: true
            qbs.installDir: bundle.isBundle ? "Applications" : (qbs.targetOS.contains("windows") ? "" : "bin")
            qbs.installSourceBase: product.buildDirectory
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }

        name: "coreutils"
        targetName: bundle.isBundle ? "CoreUtils" : "coreutils"
        files: ["coreutils.cpp", "coreutils.h"]

        Group {
            fileTagsFilter: bundle.isBundle ? ["bundle.content"] : ["dynamiclibrary", "dynamiclibrary_symlink", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: bundle.isBundle ? "Library/Frameworks" : (qbs.targetOS.contains("windows") ? "" : "lib")
            qbs.installSourceBase: product.buildDirectory
        }
    }
}
