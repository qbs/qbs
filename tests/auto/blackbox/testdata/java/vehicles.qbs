import qbs
import qbs.FileInfo

Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "car_jar" }
        bundle.isBundle: false

        name: "native"
        files: ["engine.c"]

        Group {
            fileTagsFilter: ["dynamiclibrary"]
            qbs.install: true
        }
    }

    JavaClassCollection {
        Depends { name: "random_stuff" }
        name: "class_collection"
        java.additionalCompilerFlags: ["-Xlint:all"]
        files: [
            "Car.java", "HelloWorld.java", "Jet.java", "NoPackage.java", "Ship.java",
            "Vehicle.java", "Vehicles.java"
        ]
    }

    JavaJarFile {
        name: "random_stuff"
        files: ["RandomStuff.java"]

        Group {
            fileTagsFilter: ["java.jar"]
            qbs.install: true
        }
    }

    JavaJarFile {
        name: "car_jar"
        files: ["Car.java", "Vehicle.java"]

        Export {
            Depends { name: "cpp" }
            cpp.systemIncludePaths: {
                var paths = java.jdkIncludePaths;
                if (java.compilerVersionMinor >= 8) {
                    paths.push(product.buildDirectory); // generated JNI headers
                }

                return paths;
            }
        }

        Group {
            fileTagsFilter: ["java.jar"]
            qbs.install: true
        }
    }

    JavaJarFile {
        Depends { name: "random_stuff" }
        Depends { name: "car_jar" }
        Depends { name: "native" }
        name: "jar_file"
        entryPoint: "Vehicles"
        files: ["Jet.java", "Ship.java", "Vehicles.java"]

        Group {
            fileTagsFilter: ["java.jar"]
            qbs.install: true
        }
    }
}
