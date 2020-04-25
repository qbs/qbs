Project {
    readonly property string beginning: "beginning"
    CppApplication {
        name: "app1"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        moduleProviders.mygenerator.chooseLettersFrom: project.beginning
        files: "main.cpp"
    }
    CppApplication {
        readonly property string end: "end"
        name: "app2"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        Profile {
            name: "myProfile"
            moduleProviders.mygenerator.chooseLettersFrom: product.end
        }
        qbs.profile: "myProfile"
        files: "main.cpp"
    }
}
