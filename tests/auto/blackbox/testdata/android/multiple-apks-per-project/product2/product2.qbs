import qbs

Project {
    DynamicLibrary {
        name: "p2lib1"
        files: ["lib1.cpp"]
        cpp.useRPaths: false
    }

    DynamicLibrary {
        name: "p2lib2"
        files: ["lib2.cpp"]
        Android.ndk.appStl: "stlport_shared"
    }

    AndroidApk {
        name: "twolibs2"
        packageName: "io.qt.dummy2"
        sourcesDir: "src"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
