import qbs

Project {
    DynamicLibrary {
        name: "p1lib1"
        files: ["lib1.cpp"]
        Android.ndk.appStl: "stlport_shared"
        architectures: ["mips", "x86"]
        cpp.useRPaths: false
    }

    DynamicLibrary {
        name: "p1lib2"
        files: ["lib2.cpp"]
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    AndroidApk {
        name: "twolibs1"
        packageName: "io.qt.dummy1"
        sourcesDir: "src"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
