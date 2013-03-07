import qbs 1.0

Project {
    Product {
        type: "application"
        name: "QtHelloWorld"

        Depends {
            name: "Qt"
            submodules: ["gui"]
        }

        property bool someCustomFeature: true

        Group {
            condition: qbs.debugInformation === false
            files : [ "main.cpp" ]
        }

        Group {
            condition: qbs.debugInformation === true
            files : [ "main_dbg.cpp" ]
        }

        files : [
            "object1.h",
            "object1.cpp",
            "object2.cpp",
            "foo.c"
        ]

        Group {
            files: [ "object2.h" ]
            fileTags: [ "hpp" ]
        }

        Group {
            condition: someCustomFeature == true
            files: [ "somecustomfeature.cpp" ]
        }
    }
}

