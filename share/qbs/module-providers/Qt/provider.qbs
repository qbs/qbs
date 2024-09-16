import "setup-qt.js" as SetupQt
import qbs.Probes

ModuleProvider {
    Probes.QmakeProbe {
        condition: moduleName.startsWith("Qt.")
        id: probe
        qmakePaths: qmakeFilePaths
    }
    property stringList qmakeFilePaths
    readonly property varList _qtInfos: probe.qtInfos
    condition: probe.found
    isEager: false
    relativeSearchPaths: SetupQt.doSetup(moduleName, _qtInfos, outputBaseDir, path)
}
