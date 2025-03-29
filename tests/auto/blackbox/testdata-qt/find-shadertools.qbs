import qbs.Utilities

CppApplication {
    name: "check_shadertools"
    Depends { name: "Qt.core" }
    Depends { name: "Qt.shadertools"; required: false }
    property bool dummy: {
        console.info("is qt6: " + JSON.stringify(Utilities.versionCompare(Qt.core.version, "6.0") >= 0));
        console.info("is static qt: " + JSON.stringify(Qt.core.staticBuild));
        console.info("is shadertools present: " + JSON.stringify(Qt.shadertools.present));
        console.info("has viewCount: " + JSON.stringify(Utilities.versionCompare(Qt.core.version, "6.7.0") >= 0));
    }
}