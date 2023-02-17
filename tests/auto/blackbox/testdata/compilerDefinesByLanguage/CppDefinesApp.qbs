CppApplication {
    files: ["app.c"]

    property bool enableObjectiveC: qbs.targetOS.includes("darwin")

    Group {
        name: "C/C++"
        files: [
            "test.c",
            "test.cpp",
        ]
    }

    Group {
        name: "Objective-C"
        condition: enableObjectiveC
        files: [
            "test.m",
            "test.mm",
        ]
    }
}
