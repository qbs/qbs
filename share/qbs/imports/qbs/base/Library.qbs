Product {
    type: {
        if (qbs.targetOS.contains("ios") && parseInt(cpp.minimumIosVersion, 10) < 8)
            return ["staticlibrary"];
        return ["dynamiclibrary"];
    }
}
