Product {
    type: {
        if (isForAndroid && !consoleApplication)
            return ["dynamiclibrary"];
        return ["application"];
    }

    property bool isForAndroid: qbs.targetOS.contains("android")
    property stringList architectures: isForAndroid ? ["armeabi"] : undefined

    Depends { name: "Android.ndk"; condition: isForAndroid }
    Depends { name: "bundle" }
    Depends { name: "cpp"; condition: isForAndroid }

    profiles: isForAndroid
        ? architectures.map(function(arch) { return project.profile + '_' + arch; })
        : [project.profile]
}
