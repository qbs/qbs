Project {
    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "lib1"
        files: ["src/main/jni/lib1.cpp"]
        qbs.targetPlatform: "android"
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    DynamicLibrary {
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        name: "lib2"
        files: ["src/main/jni/lib2.cpp"]
        qbs.targetPlatform: "android"
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "stlport_shared"
        cpp.useRPaths: false
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        Depends { name: "lib6" }
        Depends { name: "lib8" }
        name: "lib3"
        files: ["io/qbs/lib3/lib3.java"]
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        name: "lib4"
        files: ["lib4.java"]
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        name: "lib5"
        files: ["lib5.java"]
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        name: "lib6"
        files: ["lib6.java"]
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        name: "lib7"
        files: ["lib7.java"]
    }

    JavaJarFile {
        Depends { name: "Android.sdk" }
        Android.sdk.packageName: undefined
        Android.sdk.automaticSources: false
        Depends { name: "lib7"; Android.sdk.embedJar: false }
        name: "lib8"
        files: ["lib8.java"]
    }

    Application {
        name: "twolibs"
        Android.sdk.apkBaseName: name
        Android.sdk.packageName: "io.qt.dummy"
        Depends { productTypes: ["android.nativelibrary"] }
        Depends { name: "lib3"; Android.sdk.embedJar: true }
        Depends { name: "lib4"; Android.sdk.embedJar: false }
        Depends { name: "lib5" }
    }
}
