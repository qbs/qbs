import qbs.File

Project {
    minimumQbsVersion: qbs.version
    StaticLibrary {
        name: "native-glue"
        qbs.targetPlatform: "android"
        cpp.warningLevel: "none"
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
        qbs.targetPlatform: "android"
        cpp.warningLevel: "none"
        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        Depends { name: "native-glue" }

        Probe {
            id: ndkHelperProbe
            property string ndkDir: Android.ndk.ndkDir
            property string samplesDir: Android.ndk.ndkSamplesDir
            property string dir
            configure: {
                var paths = [samplesDir + "/teapots/common/ndk_helper/",
                             ndkDir + "/sources/android/ndk_helper/"];
                for (var i = 0; i < paths.length; ++i) {
                    if (File.exists(paths[i])) {
                        dir = paths[i];
                        break;
                    }
                }
            }
        }

        Group {
            id: ndkhelper_sources
            prefix: ndkHelperProbe.dir
            files: ["*.cpp", "*.h"].concat(
                !File.exists(ndkHelperProbe.dir + "/gl3stub.cpp") ? ["gl3stub.c"] : [])
        }
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "gnustl_shared"
        cpp.cxxLanguageVersion: "c++11"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [ndkhelper_sources.prefix]
            cpp.dynamicLibraries: ["log", "android", "EGL", "GLESv2"]
        }
    }

    StaticLibrary {
        name: "android_cpufeatures"
        qbs.targetPlatform: "android"
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

    CppApplication {
        name: "TeapotNativeActivity"
        qbs.targetPlatform: "android"

        Depends { name: "Android.ndk" }
        Depends { name: "cpp" }
        Depends { name: "android_cpufeatures" }
        Depends { name: "native-glue" }
        Depends { name: "ndk-helper" }

        Probe {
            id: teapotProbe
            property string samplesDir: Android.sdk.ndkSamplesDir
            property string dir
            configure: {
                var paths = ["/teapots/classic-teapot/src/main", "/Teapot/app/src/main", "/Teapot"];
                for (var i = 0; i < paths.length; ++i) {
                    if (File.exists(samplesDir + paths[i])) {
                        dir = samplesDir + paths[i];
                        break;
                    }
                }
            }
        }

        Probe {
            id: teapotProbeJni
            property string samplesDir: Android.ndk.ndkSamplesDir
            property string jniDir
            configure: {
                var paths = ["/teapots/classic-teapot/src/main/cpp/", "/Teapot/app/src/main/jni/",
                             "/Teapot/jni/"];
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

        Android.sdk.apkBaseName: name
        Android.sdk.packageName: "com.sample.teapot"
        Android.sdk.sourceSetDir: teapotProbe.dir
        Properties { condition: qbs.toolchain.contains("clang"); Android.ndk.appStl: "c++_shared" }
        Android.ndk.appStl: "gnustl_shared"
        cpp.cxxLanguageVersion: "c++11"
        cpp.dynamicLibraries: ["log", "android", "EGL", "GLESv2"]
        cpp.useRPaths: false

        // Export ANativeActivity_onCreate(),
        // Refer to: https://github.com/android-ndk/ndk/issues/381
        cpp.linkerFlags: ["-u", "ANativeActivity_onCreate"]
    }
}
