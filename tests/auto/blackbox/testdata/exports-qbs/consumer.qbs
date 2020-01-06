CppApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    name: "consumer"
    qbsSearchPaths: "default/install-root/usr/qbs"
    property string outTag: "cpp"
    Depends { name: "MyLib" }
    Depends { name: "MyTool" }
    files: ["consumer.cpp"]
    cpp.defines: {
        var defs = [];
        if (MyLib.config.feature_x)
            defs.push("FEATURE_X");
        if (MyLib.config.feature_y)
            defs.push("FEATURE_Y");
        return defs;
    }

    Group {
        files: ["helper.cpp.in"]
        fileTags: ["cpp.in"]
    }
}
