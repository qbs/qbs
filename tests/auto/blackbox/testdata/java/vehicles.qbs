import qbs

Project {
    JavaClassCollection {
        name: "class_collection"
        java.additionalCompilerFlags: ["-Xlint:all"]
        files: [
            "Car.java", "HelloWorld.java", "Jet.java", "NoPackage.java", "Ship.java",
            "Vehicle.java", "Vehicles.java"
        ]
    }

    JavaJarFile {
        name: "jar_file"
        entryPoint: "Vehicles"
        files: ["Car.java", "Jet.java", "Ship.java", "Vehicle.java", "Vehicles.java"]
    }
}
