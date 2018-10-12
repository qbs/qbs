Project {
    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "p1lib1"
        files: ["src/main/jni/lib1.cpp"]
        qbs.targetPlatform: "android"
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "stlport_shared"
        qbs.architectures: !qbs.architecture ? ["armv7a", "x86"] : undefined
        cpp.useRPaths: false
    }

    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "p1lib2"
        files: ["src/main/jni/lib2.cpp"]
        qbs.targetPlatform: "android"
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    Application {
        name: "twolibs1"
        Android.sdk.apkBaseName: name
        Android.sdk.packageName: "io.qt.dummy1"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
