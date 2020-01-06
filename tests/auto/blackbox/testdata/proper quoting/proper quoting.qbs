import qbs 1.0

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        type: "application"
        consoleApplication: true
        name: "Hello World"
        files : [ "main.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "my static lib" }
        cpp.defines: [
            'DEFINE="whitespaceless"',
            'DEFINEWITHSPACE="contains space"',
            'DEFINEWITHTAB="contains\ttab"',
            'DEFINEWITHBACKSLASH="backslash\\\\"',
        ]
    }

    Product {
        type: "staticlibrary"
        name : "my static lib"
        files : [ "my static lib.cpp" ]
        Depends { name: "cpp" }
        Depends { name: "helper lib" }
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
            cpp.includePaths: [product.sourceDirectory + '/some helper']
        }
    }
}
