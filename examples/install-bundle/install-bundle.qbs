import qbs
import qbs.FileInfo

Project {
    CppApplication {
        Depends { name: "coreutils" }
        Depends { name: "Qt"; submodules: ["core", "gui", "widgets"] }

        name: "window"
        targetName: bundle.isBundle ? "Window" : "window"
        files: ["main.cpp"]

        property bool install: true
        property string installDir: bundle.isBundle ? "Applications" : (qbs.targetOS.contains("windows") ? "" : "bin")

        Group {
            fileTagsFilter: ["application"]
            qbs.install: install
            qbs.installDir: bundle.isBundle ? FileInfo.joinPaths(installDir, FileInfo.path(bundle.executablePath)) : installDir
        }

        Group {
            fileTagsFilter: ["infoplist"]
            qbs.install: install && bundle.isBundle && !bundle.embedInfoPlist
            qbs.installDir: FileInfo.joinPaths(installDir, FileInfo.path(bundle.infoPlistPath))
        }

        Group {
            fileTagsFilter: ["pkginfo"]
            qbs.install: install && bundle.isBundle
            qbs.installDir: FileInfo.joinPaths(installDir, FileInfo.path(bundle.pkgInfoPath))
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }

        name: "coreutils"
        targetName: bundle.isBundle ? "CoreUtils" : "coreutils"
        files: ["coreutils.cpp", "coreutils.h"]

        property bool install: true
        property string installDir: bundle.isBundle ? "Library/Frameworks" : (qbs.targetOS.contains("windows") ? "" : "lib")

        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_symlink", "dynamiclibrary_import"]
            qbs.install: install
            qbs.installDir: bundle.isBundle ? FileInfo.joinPaths(installDir, FileInfo.path(bundle.executablePath)) : installDir
        }

        Group {
            fileTagsFilter: ["infoplist"]
            qbs.install: install && bundle.isBundle && !bundle.embedInfoPlist
            qbs.installDir: FileInfo.joinPaths(installDir, FileInfo.path(bundle.infoPlistPath))
        }
    }
}
