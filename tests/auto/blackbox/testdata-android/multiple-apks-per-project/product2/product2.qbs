Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "p2lib1"
        files: ["src/main/jni/lib1.cpp"]
        qbs.targetPlatform: "android"
        cpp.useRPaths: false
    }

    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "p2lib2"
        files: ["src/main/jni/lib2.cpp"]
        qbs.targetPlatform: "android"
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "stlport_shared"
    }

    Application {
        name: "twolibs2"
        Android.sdk.apkBaseName: name
        Android.sdk.packageName: "io.qt.dummy2"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
