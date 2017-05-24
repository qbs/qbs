import qbs

Project {
    CppApplication { files: ["main1.cpp"] }
    CppApplication {
        qbs.profiles: ["qbs-autotests-subprofile"]
        files: ["main2.cpp"]
    }
}
