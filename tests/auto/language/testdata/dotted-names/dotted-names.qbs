import qbs

Project {
    name: "theProject"
    property bool includeDottedProduct
    property bool includeDottedModule

    Project {
        condition: project.includeDottedProduct
        Product {
            name: "a.b"
            Export { property string c: "default" }
        }
    }

    Product {
        name: "p"
        Depends { name: "a.b"; condition: project.includeDottedProduct }
        Depends { name: "x.y"; condition: project.includeDottedModule }
        a.b.c: "p"
        x.y.z: "p"
    }
}

