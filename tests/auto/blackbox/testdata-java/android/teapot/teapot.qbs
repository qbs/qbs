import qbs
import qbs.File

Project {
    minimumQbsVersion: qbs.version
    StaticLibrary {
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
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        Depends { name: "cpufeatures" }
        Depends { name: "native-glue" }
        Depends { name: "ndk-helper" }

        Probe {
            id: teapotProbeJni
            property string samplesDir: Android.ndk.ndkSamplesDir
            property string jniDir
            configure: {
                var paths = ["/teapots/classic-teapot/src/main/cpp/", "/Teapot/app/src/main/jni/"];
                for (var i = 0; i < paths.length; ++i) {
                    if (File.exists(samplesDir + paths[i])) {
                        jniDir = samplesDir + paths[i];
                        break;
                    }
                }
            }
        }

        Group {
            name: "C++ sources"
            prefix: teapotProbeJni.jniDir
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
        Probe {
            id: teapotProbe
            property string samplesDir: Android.sdk.ndkSamplesDir
            property string dir
            configure: {
                var paths = ["/teapots/classic-teapot/src/main", "/Teapot/app/src/main"];
                for (var i = 0; i < paths.length; ++i) {
                    if (File.exists(samplesDir + paths[i])) {
                        dir = samplesDir + paths[i];
                        break;
                    }
                }
            }
        }

        name: "com.sample.teapot"
        sourceSetDir: teapotProbe.dir
        Depends { productTypes: ["android.nativelibrary"] }
    }
}
