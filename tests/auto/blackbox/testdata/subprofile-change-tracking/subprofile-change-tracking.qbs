import qbs

Project {
    CppApplication { files: ["main1.cpp"] }
    CppApplication {
        profiles: ["qbs-autotests-subprofile"]
        files: ["main2.cpp"]
    }
}
