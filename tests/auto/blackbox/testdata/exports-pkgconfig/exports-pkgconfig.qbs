import qbs.FileInfo

Project {
    Product {
        name: "dummy"
        Export { Depends { name: "TheFirstLib" } }
    }

    Product {
        name: "SomeHelper"
        Depends { name: "Exporter.pkgconfig" }
        Exporter.pkgconfig.versionEntry: "1.0"
    }
    StaticLibrary {
        Depends { name: "cpp" }
        name: "BoringStaticLib"
        files: ["boringstaticlib.cpp"]
        Export {
            Depends { name: "cpp" }
            cpp.defines: ["HAVE_INDUSTRIAL_STRENGTH_HAIR_DRYER"]
        }
    }
    DynamicLibrary {
        name: "TheFirstLib"
        version: "1.0"

        Depends { name: "SomeHelper" }
        Depends { name: "Exporter.pkgconfig" }
        Exporter.pkgconfig.excludedDependencies: ["Qt.core", "helper3"]
        Exporter.pkgconfig.requiresEntry: "Qt5Core"
        Exporter.pkgconfig.urlEntry: "http://www.example.com/thefirstlib"

        Depends { name: "cpp" }
        cpp.defines: ["FIRSTLIB"]

        qbs.installPrefix: "/opt/the firstlib"

        Export {
            prefixMapping: [{prefix: "/somedir", replacement: "/otherdir"}]
            Depends { name: "BoringStaticLib" }
            Depends { name: "cpp" }
            Depends { name: "Qt.core"; required: false }
            Depends { name: "helper1" }
            Depends { name: "helper3" }
            property bool someCondition: qbs.hostOS.contains("windows") // hostOS for easier testing
            property bool someOtherCondition: someCondition
            Properties {
                condition: !someOtherCondition
                cpp.driverFlags: ["-pthread"]
            }
            cpp.defines: product.name
            cpp.includePaths: [FileInfo.joinPaths(product.qbs.installPrefix, "include")]
            Qt.core.mocName: "muck"
        }

        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "lib"
        }

        Group {
            name: "api_headers"
            files: ["firstlib.h"]
            qbs.install: true
            qbs.installDir: "include"
        }

        files: ["firstlib.cpp"]
    }
    DynamicLibrary {
        name: "TheSecondLib"
        version: "2.0"

        Depends { name: "Exporter.pkgconfig" }
        Exporter.pkgconfig.descriptionEntry: "The second lib"
        Exporter.pkgconfig.transformFunction: (function(product, moduleName, propertyName, value) {
            if (moduleName === "cpp" && propertyName === "includePaths")
                return value.filter(function(p) { return p !== product.sourceDirectory; });
            return value;
        })
        Exporter.pkgconfig.customVariables: ({config1: "a b", config2: "c"})

        Depends { name: "cpp" }
        cpp.defines: ["SECONDLIB"]

        qbs.installPrefix: ""

        Depends { name: "TheFirstLib" }

        Export {
            Depends { name: "TheFirstLib" }
            Depends { name: "dummy" }
            Depends { name: "cpp" }
            cpp.includePaths: [
                "/opt/thesecondlib/include",
                product.sourceDirectory,
                importingProduct.buildDirectory
            ]
            property string hurz: importingProduct.name
            cpp.defines: hurz.toUpperCase()

            Rule {
                property int n: 5
                Artifact {
                    filePath: "dummy"
                    fileTags: ["d1", "d2"]
                    cpp.warningsAreErrors: true
                }
            }
        }

        Group {
            fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_import"]
            qbs.install: true
            qbs.installDir: "/opt/thesecondlib/lib"
        }

        Group {
            name: "api_headers"
            files: ["secondlib.h"]
            qbs.install: true
            qbs.installDir: "/opt/thesecondlib/include"
        }

        files: ["secondlib.cpp"]
    }
}
