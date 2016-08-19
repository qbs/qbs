import qbs

Project {
    minimumQbsVersion: qbs.version
    property stringList architectures: ["arm64", "armv7a", "x86", "x86_64", "mips", "mips64"]
    StaticLibrary {
        architectures: project.architectures
        name: "native-glue"
        Depends { name: "cpp" }
        Group {
            id: glue_sources
            prefix: Android.ndk.ndkDir + "/sources/android/native_app_glue/"
            files: ["*.c", "*.h"]
        }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [glue_sources.prefix]
            cpp.dynamicLibraries: ["log"]
        }
    }

    StaticLibrary {
        architectures: project.architectures
        name: "ndk-helper"
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        Depends { name: "native-glue" }

        Group {
            id: ndkhelper_sources
            prefix: Android.ndk.ndkDir + "/sources/android/ndk_helper/"
            files: ["*.c", "*.cpp", "*.h"]
        }
        Android.ndk.appStl: "stlport_shared"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [ndkhelper_sources.prefix]
            cpp.dynamicLibraries: ["log", "android", "EGL", "GLESv2"]
        }
    }

    StaticLibrary {
        architectures: project.architectures
        name: "cpufeatures"
        Depends { name: "cpp" }
        Group {
            id: cpufeatures_sources
            prefix: Android.ndk.ndkDir + "/sources/android/cpufeatures/"
            files: ["*.c", "*.h"]
        }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [cpufeatures_sources.prefix]
            cpp.dynamicLibraries: ["dl"]
        }
    }

    DynamicLibrary {
        name: "TeapotNativeActivity"
        architectures: project.architectures
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        Depends { name: "cpufeatures" }
        Depends { name: "native-glue" }
        Depends { name: "ndk-helper" }

        Group {
            name: "C++ sources"
            prefix: Android.ndk.ndkDir + "/samples/Teapot/app/src/main/jni/"
            files: [
                "TeapotNativeActivity.cpp",
                "TeapotRenderer.cpp",
                "TeapotRenderer.h",
                "teapot.inl",
            ]
        }

        FileTagger { patterns: ["*.inl"]; fileTags: ["hpp"] }

        Android.ndk.appStl: "stlport_shared"
        cpp.dynamicLibraries: ["log", "android", "EGL", "GLESv2"]
        cpp.useRPaths: false
    }

    AndroidApk {
        name: "com.sample.teapot"
        sourceSetDir: Android.sdk.ndkDir + "/samples/Teapot/app/src/main"
        Depends { productTypes: ["android.nativelibrary"] }
    }
}
