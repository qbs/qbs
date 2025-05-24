import qbs.Host

Product {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }

    name: "lrelease-test"
    type: ["ts"]
    Depends { name: "Qt.core" }
    files: ["de.ts", "hu.ts"]
}
