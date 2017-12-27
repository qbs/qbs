QbsLibraryBase {
    staticBuild: true
    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }

        cpp.includePaths: [product.sourceDirectory]
    }
}
