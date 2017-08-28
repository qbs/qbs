import qbs

Project {
    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "p1lib1"
        files: ["src/main/jni/lib1.cpp"]
        Android.ndk.appStl: "stlport_shared"
        qbs.architectures: !qbs.architecture ? ["mips", "x86"] : undefined
        cpp.useRPaths: false
    }

    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "p1lib2"
        files: ["src/main/jni/lib2.cpp"]
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    AndroidApk {
        name: "twolibs1"
        packageName: "io.qt.dummy1"
        Depends {
            productTypes: ["android.nativelibrary"]
            limitToSubProject: true
        }
    }
}
