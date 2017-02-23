import qbs

CppApplication {
    consoleApplication: true
    files: ["main.cpp"]
    Group {
        fileTagsFilter: ["cpp"]
        qbs.install: true
    }
    Group { // this overwrites the properties of the group below
        fileTagsFilter: ["text"]
        qbs.install: true
    }
    Group {
        files: ["readme.txt"]
        fileTags: ["text"]
        qbs.install: false
    }
}

