import qbs

Project {
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
    }

    JavaJarFile {
        Depends { name: "random_stuff" }
        name: "jar_file"
        entryPoint: "Vehicles"
        files: ["Car.java", "Jet.java", "Ship.java", "Vehicle.java", "Vehicles.java"]
    }
}
