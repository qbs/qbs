import qbs.Host
import qbs.Utilities

Project {
    // "linked": dynamic plugin, linked directly by the app (omitPluginEntry: true)
    // "unlinked": dynamic plugin, loaded at runtime via QML import path
    // "static": static plugin, linked into the app with --whole-archive
    property string mode: "linked"

    QmlModule {
        name: "myqmlplugin"
        uri: "io.qt.QbsTest"
        version: "1.0"
        Properties {
            condition: mode === "static"
            type: "staticlibrary"
        }
        Qt.qml.pluginClassName: "QbsTestPlugin"
        modulesInstallDir: "qml/io/qt/QbsTest"
        qbs.installPrefix: ""
        Qt.qml.loadedAtRuntime: mode === "unlinked"
        cpp.includePaths: [sourceDirectory]

        property string mode: versionCheck.mode

        files: [
            "MyQmlItem.qml",
            "myitem.cpp",
            "myitem.h",
            "qbstestplugin.cpp",
        ]

        Probe {
            id: versionCheck
            property string qtVersion: Qt.core.version
            property bool staticQt: Qt.core.staticBuild
            property string mode: project.mode
            configure: {
                if (Utilities.versionCompare(qtVersion, "5.15") < 0)
                    console.info("typeRegistrar not available");
                if (Utilities.versionCompare(qtVersion, "6.2") >= 0)
                    console.info("prefer supported");
                if (staticQt) {
                    mode = "static";
                    console.info("static Qt");
                }
                found = true;
            }
        }
    }

    CppApplication {
        name: "app"
        condition: {
            if (qbs.targetPlatform !== Host.platform()
                    || qbs.architecture !== Host.architecture()) {
                console.info("target platform/arch differ from host platform/arch");
                return false;
            }
            return true;
        }
        qbs.installPrefix: ""
        installDir: "bin"

        Depends { name: "Qt.qml" }
        Depends {
            name: "myqmlplugin"
            condition: project.mode !== "unlinked" || Qt.core.staticBuild
        }

        files: "main.cpp"
    }
}
