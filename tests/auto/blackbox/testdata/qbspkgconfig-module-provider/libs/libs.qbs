import qbs.FileInfo

Project {
    property bool isBundle: false

    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "libA"
        bundle.isBundle: project.isBundle
        bundle.publicHeaders: ["libA.h"]
        files: "libA.cpp"
        cpp.defines: {
            var result = [];
            if (project.isBundle)
                result.push("MYLIB_FRAMEWORK");
            return result;
        }
        qbs.installPrefix: ""
        install: true
        installImportLib: true
        installDir: "lib"
        Group {
            files: ["libA.h"]
            qbs.install: !project.isBundle
            qbs.installDir: FileInfo.joinPaths("include", product.name)
        }
    }
}
