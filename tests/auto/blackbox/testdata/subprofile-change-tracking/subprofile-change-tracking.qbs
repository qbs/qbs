Project {
    CppApplication { name: "app1"; files: ["main1.cpp"] }
    CppApplication {
        name: "app2"
        qbs.profiles: ["qbs-autotests-subprofile"]
        files: ["main2.cpp"]
    }
}
