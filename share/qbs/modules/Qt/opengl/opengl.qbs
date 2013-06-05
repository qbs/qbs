import qbs 1.0
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: 'OpenGL'
    Properties {
        condition: Qt.core.staticBuild && qbs.targetOS.contains("ios")
        cpp.frameworks: base.concat(["OpenGLES", "QuartzCore", "CoreGraphics"])
    }
    cpp.frameworks: base
}

