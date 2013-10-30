import qbs 1.0

Application {
    name : "CollidingMice"
    destinationDirectory: "bin"

    Depends {
        name: "Qt"
        submodules: ["widgets"]
    }

    files : [
        "main.cpp",
        "mouse.cpp",
        "mouse.h",
        "mice.qrc"
    ]
}

