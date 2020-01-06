Project {
    property bool enabled: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    CppApplication {
        name: "app1"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        moduleProviders.mygenerator.chooseLettersFrom: "beginning"
        files: "main.cpp"
    }
    CppApplication {
        name: "app2"
        Depends { name: "mygenerator.module1" }
        Depends { name: "mygenerator.module2" }
        Profile {
            name: "myProfile"
            moduleProviders.mygenerator.chooseLettersFrom: "end"
        }
        qbs.profile: "myProfile"
        files: "main.cpp"
    }
}
