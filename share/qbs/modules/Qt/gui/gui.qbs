import qbs 1.0
import qbs.FileInfo
import '../QtModule.qbs' as QtModule
import '../../utils.js' as ModUtils

QtModule {
    qtModuleName: "Gui"

    property string uicName: "uic"

    FileTagger {
        pattern: "*.ui"
        fileTags: ["ui"]
    }

    Rule {
        inputs: ["ui"]

        Artifact {
//  ### TODO we want to access the module's property "Qt.core.generatedFilesDir" here. But without evaluating all available properties a priori.
            fileName: 'GeneratedFiles/' + product.name + '/ui_' + input.completeBaseName + '.h'
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(ModUtils.moduleProperty(product, "binPath") + '/'
                                  + ModUtils.moduleProperty(product, "uicName"),
                                  [input.fileName, '-o', output.fileName])
            cmd.description = 'uic ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    cpp.includePaths: Qt.core.qtConfig.contains("angle")
                      ? base.concat([FileInfo.joinPaths(incPath, "QtANGLE")]) : base

    Properties {
        condition: Qt.core.staticBuild && qbs.targetOS.contains("ios")
        cpp.frameworks: base.concat(["UIKit", "QuartzCore", "CoreText", "CoreGraphics",
                                     "Foundation", "CoreFoundation"])
    }
    cpp.frameworks: base
}

