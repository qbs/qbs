Project {
    property string packageBaseName

    Product {
        name: "theProduct1"
        type: ["theType"]

        Depends { name: "themodule" }
        qbs.sysroot: path + "/sysroot1"
    }

    Product {
        name: "theProduct2"
        type: ["theType"]

        Depends { name: "themodule" }
        qbs.sysroot: path + "/sysroot2"
    }

    Product {
        name: "theProduct3"
        type: ["theType"]

        Depends { name: "themodule" }
        qbs.sysroot: path + "/sysroot1"
    }
}
