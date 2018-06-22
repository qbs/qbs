import "exports_product.qbs" as ProductWithInheritedExportItem

Project {
    Application {
        name: "myapp"
        Depends { name: "mylib" }
        Depends { name: "dummy" }
        Depends { name: "qbs" }
        dummy.defines: ["BUILD_" + product.name.toUpperCase()]
        dummy.includePaths: ["./app"]
    }

    references: [
        "subdir/exports-mylib.qbs",
        "subdir2/exports-mylib2.qbs"
    ]

    Application {
        name: "A"
        Depends { name: "qbs" }
        Depends { name: "B" }
    }
    StaticLibrary {
        name: "B"
        Export {
            Depends { name: "C" }
            Depends { name: "qbs" }
        }
    }
    StaticLibrary {
        name: "C"
        Export {
            Depends { name: "D" }
            Depends { name: "qbs" }
        }
    }
    StaticLibrary {
        name: "D"
    }

    Application {
        name: "myapp2"
        Depends { name: "productWithInheritedExportItem" }
        Depends { name: "qbs" }
    }
    ProductWithInheritedExportItem {
        name: "productWithInheritedExportItem"
        Export {
            dummy.cFlags: base.concat("PRODUCT_" + product.name.toUpperCase())
            dummy.cxxFlags: ["-bar"]
            Properties {
                condition: true
                dummy.defines: base.concat(["DEF"])
            }
        }
    }
    Application {
        name: "myapp3"
        Depends { name: "productWithInheritedExportItem"; versionAtLeast: "2.0" }
    }

    Project {
        name: "sub1"
        Product {
            name: "sub p1"
            Export {
                Depends { name: "dummy" }
                dummy.someString: project.name
            }
        }
    }

    Project {
        name: "sub2"
        Product {
            name: "sub p2"
            Depends { name: "sub p1" }
        }
    }

    ParentWithExport {
        name: "libA"
        Export { Depends { name: "libB" } }
    }

    ParentWithExport { name: "libB" }

    ParentWithExport {
        name: "libC"
        Export { Depends { name: "libA" } }
    }

    ParentWithExport {
        name: "libD"
        Export { Depends { name: "libA" } }
    }

    Product {
        name: "libE"

        Depends { name: "libD" }
        Depends { name: "libC" }

        Group {
            qbs.install: false
        }
    }

    Product {
        name: "dependency"

        Probe {
            id: configProbe
            property var config
            configure: {
                var obj = {};
                obj.featureX = true;
                obj.featureY = false;
                obj.featureZ = true;
                config = obj;
                found = true;
            }
        }
        property var config: configProbe.config

        Export {
            property bool depend: false
            property var config: product.config
            Depends { condition: depend; name: "cpp" }
            Properties { condition: depend; cpp.includePaths: ["."] }
        }
    }
    Product {
        name: "depender"
        Depends { name: "dependency" }
        property bool featureX: dependency.config.featureX
        property bool featureY: dependency.config.featureY
        property bool featureZ: dependency.config.featureZ
    }

    Product {
        name: "broken_cycle1"
        Export {
            property bool depend: true
            Depends { name: "broken_cycle3"; condition: depend }                                                                                                                                                                                                }
        }
    Product {
        name: "broken_cycle2"
        Export {
            Depends { name: "broken_cycle1" }
            broken_cycle1.depend: false
       }
    }
    Product {
        name: "broken_cycle3"
        Depends { name: "broken_cycle2" }
    }
}
