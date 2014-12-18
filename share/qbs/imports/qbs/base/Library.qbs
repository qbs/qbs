Product {
    type: {
        if (qbs.targetOS.contains("ios") && parseInt(cpp.minimumIosVersion, 10) < 8)
            return ["staticlibrary"];
        return ["dynamiclibrary"].concat(isForAndroid ? ["android.nativelibrary"] : []);
    }

    property bool isForAndroid: qbs.targetOS.contains("android")
    property stringList architectures: isForAndroid ? ["armeabi"] : undefined

    Depends { name: "Android.ndk"; condition: isForAndroid }
    Depends { name: "cpp"; condition: isForAndroid }

    profiles: isForAndroid
        ? architectures.map(function(arch) { return project.profile + '_' + arch; })
        : [project.profile]
}
