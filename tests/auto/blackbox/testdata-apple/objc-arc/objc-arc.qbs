Product {
    Depends { name: "cpp" }
    consoleApplication: true
    type: ["application"]
    condition: qbs.targetOS.contains("darwin")

    Group {
        cpp.automaticReferenceCounting: true
        files: ["arc.m", "arc.mm"]
    }

    Group {
        cpp.automaticReferenceCounting: false
        files: ["mrc.m", "mrc.mm"]
    }

    files: "main.m"
    cpp.minimumMacosVersion: "10.7"
}
