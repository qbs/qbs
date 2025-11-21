Project {
    CppApplication {
        name: "app1"
        consoleApplication: true
        files: ["main1.cpp"]
    }
    CppApplication {
        name: "app2"
        consoleApplication: true
        qbs.profiles: ["qbs-autotests-subprofile"]
        files: ["main2.cpp"]
    }
}
