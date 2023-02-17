Project {
    property string binPath: "/usr/bin"
    property string libPath: "/usr/lib"

    Properties {
        condition: qbs.targetOS.includes("macos")
        binPath: "/Users/boo"
        libPath: "/Libraries/foo"
    }
}
