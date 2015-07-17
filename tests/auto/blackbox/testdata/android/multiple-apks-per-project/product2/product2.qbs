import qbs

Project {
    DynamicLibrary {
        name: "p2lib1"
        files: ["src/main/jni/lib1.cpp"]
        cpp.useRPaths: false
    }

    DynamicLibrary {
        name: "p2lib2"
        files: ["src/main/jni/lib2.cpp"]
        Android.ndk.appStl: "stlport_shared"
    }

    AndroidApk {
        name: "twolibs2"
        packageName: "io.qt.dummy2"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
