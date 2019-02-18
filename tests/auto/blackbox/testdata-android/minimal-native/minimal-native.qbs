CppApplication {
    name: "minimalnative"
    qbs.buildVariant: "release"
    Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
    Android.sdk.packageName: "my.minimalnative"
    Android.sdk.apkBaseName: name
    Android.ndk.appStl: "stlport_shared"
    files: "src/main/native/native.c"
    Group {
        files: "libdependency.so"
        fileTags: "android.nativelibrary"
    }
}
