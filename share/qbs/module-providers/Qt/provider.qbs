import "setup-qt.js" as SetupQt
import qbs.Probes

ModuleProvider {
    Probes.QmakeProbe {
        id: probe
        qmakePaths: qmakeFilePaths
    }
    property stringList qmakeFilePaths
    readonly property varList _qtInfos: probe.qtInfos
    condition: moduleName.startsWith("Qt.")
    isEager: false
    relativeSearchPaths: SetupQt.doSetup(moduleName, _qtInfos, outputBaseDir, path)
}
