Project {
    property string binPath: "/usr/bin"
    property string libPath: "/usr/lib"

    Properties {
        condition: qbs.targetOS.contains("macos")
        binPath: "/Users/boo"
        libPath: "/Libraries/foo"
    }
}
