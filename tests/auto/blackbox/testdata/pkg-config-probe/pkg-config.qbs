Project {
    property string packageBaseName

    Product {
        name: "theProduct1"
        type: ["theType"]

        Depends { name: "themodule" }
        themodule.packageName: project.packageBaseName + "1"
        themodule.libDir: path + "/dummy1"
    }

    Product {
        name: "theProduct2"
        type: ["theType"]

        Depends { name: "themodule" }
        themodule.packageName: project.packageBaseName + "2"
        themodule.libDir: path + "/dummy2"
    }
}
