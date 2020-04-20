Project {
    property bool enabled: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    readonly property string beginning: "beginning"
    CppApplication {
        name: "app1"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        Depends { name: "othergenerator" }
        moduleProviders.mygenerator.chooseLettersFrom: project.beginning
        moduleProviders.othergenerator.someDefines: "app1"
        files: "main.cpp"
    }
    CppApplication {
        readonly property string end: "end"
        name: "app2"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        Depends { name: "othergenerator" }
        Profile {
            name: "myProfile"
            moduleProviders.mygenerator.chooseLettersFrom: product.end
            moduleProviders.othergenerator.someDefines: "app2"
        }
        qbs.profile: "myProfile"
        files: "main.cpp"
    }
}
