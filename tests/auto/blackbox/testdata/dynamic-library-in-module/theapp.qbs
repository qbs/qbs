import qbs

Project {
    CppApplication {
        name: "theapp"
        cpp.minimumMacosVersion: "10.5" // For -rpath
        Depends { name: "theotherlib" }
        Depends { name: "thethirdlib" }
        Depends { name: "thefourthlib" }
        files: "main.cpp"
    }
    Dll {
        name: "thefourthlib"
        Depends { name: "thelib" }
        files: "lib4.cpp"
        Export {
            Depends { name: "cpp" }
            cpp.rpaths: [qbs.installRoot]
        }
    }
}
