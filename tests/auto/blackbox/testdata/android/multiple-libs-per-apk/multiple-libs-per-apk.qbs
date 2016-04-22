import qbs

Project {
    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "lib1"
        files: ["src/main/jni/lib1.cpp"]
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "lib2"
        files: ["src/main/jni/lib2.cpp"]
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    AndroidApk {
        name: "twolibs"
        packageName: "io.qt.dummy"
        Depends { productTypes: ["android.nativelibrary"] }
    }
}
