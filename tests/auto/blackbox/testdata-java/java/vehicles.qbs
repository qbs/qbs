import qbs.FileInfo
import qbs.Utilities

Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "car_jar" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }

        name: "native"
        files: ["engine.c"]

        qbs.installPrefix: ""
        install: true
        installDir: ""
    }

    JavaClassCollection {
        Depends { name: "random_stuff" }
        name: "cc"
        java.additionalCompilerFlags: ["-Xlint:all"]
        files: [
            "Car.java", "HelloWorld.java", "Jet.java", "NoPackage.java", "Ship.java",
            "Vehicle.java", "Vehicles.java"
        ]

        Group {
            condition: Utilities.versionCompare(java.version, "1.8") >= 0
            files: ["Car8.java", "HelloWorld8.java"]
        }

        Export {
            Depends { name: "java" }
            java.manifestClassPath: [product.targetName + ".jar"]
        }
    }

    JavaJarFile {
        name: "random_stuff"
        files: ["RandomStuff.java"]

        qbs.installPrefix: ""
        Group {
            fileTagsFilter: ["java.jar"]
            qbs.install: true
        }

        Export {
            Depends { name: "java" }
            java.manifestClassPath: [product.targetName + ".jar"]
        }
    }

    JavaJarFile {
        name: "car_jar"
        files: ["Car.java", "Vehicle.java"]

        Group {
            condition: Utilities.versionCompare(java.version, "1.8") >= 0
            files: ["Car8.java"]
        }

        Export {
            Depends { name: "cpp" }
            cpp.systemIncludePaths: {
                var paths = importingProduct.java.jdkIncludePaths;
                if (Utilities.versionCompare(importingProduct.java.version, "1.8") >= 0) {
                    paths.push(product.buildDirectory); // generated JNI headers
                }
                return paths;
            }

            Depends { name: "java" }
            java.manifestClassPath: [product.targetName + ".jar"]
        }

        qbs.installPrefix: ""
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
        files: ["Jet.java", "Ship.java", "Vehicles.java", "Manifest.mf", "Manifest2.mf"]

        java.manifest: {
            var mf = original;
            mf["Extra-Property"] = "Crazy-Value";
            return mf;
        }

        qbs.installPrefix: ""
        Group {
            fileTagsFilter: ["java.jar"]
            qbs.install: true
        }
    }
}
