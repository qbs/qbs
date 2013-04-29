import qbs 1.0

Project {
    Product {
        type: "application"
        name: "Hello World"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "my static lib" }
        cpp.defines: [
            'DEFINE="whitespaceless"',
            'DEFINEWITHSPACE="contains space"',
            'DEFINEWITHTAB="contains\ttab"',
            'DEFINEWITHBACKSLASH="backslash\\\\"',
            'DEFINEWITHBACKSLASHRAW=backslash\\\\'
        ]
//        cpp.responseFileThreshold: 0
    }

    Product {
        type: "staticlibrary"
        name : "my static lib"
        files : [ "my static lib.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "helper lib" }
//        cpp.responseFileThreshold: 0
    }

    Product {
        type: "staticlibrary"
        name : "helper lib"
        files : [
            "some helper/some helper.h",
            "some helper/some helper.cpp"
        ]
        Depends { name: "cpp" }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: ['some helper']
        }
//        cpp.responseFileThreshold: 0
    }
}
