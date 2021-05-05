BareMetalProduct {
    type: "application"

    Group {
        condition: qbs.toolchain.contains("cosmic")
        files: "cosmic.lkf"
        fileTags: "linkerscript"
    }
}
