import qbs

Product {
    Depends { name: "cpp" }
    type: ["application"]
    condition: qbs.targetOS.contains("darwin")

    Group {
        cpp.useObjcAutomaticReferenceCounting: true
        files: ["arc.m", "arc.mm"]
    }

    Group {
        cpp.useObjcAutomaticReferenceCounting: false
        files: ["mrc.m", "mrc.mm"]
    }

    files: "main.m"
    cpp.minimumOsxVersion: "10.6"
}
