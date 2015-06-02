import qbs
import qbs.FileInfo
import qbs.Process
import qbs.TextFile

Project {
    property path qbsFilePath
    Product {
        Depends { name: "Qt.core" }
        type: ["dummy"]
        Transformer {
            Artifact {
                filePath: "dummy.txt"
                fileTags: ["dummy"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    // Synchronous run, successful.
                    var process = new Process();
                    process.setEnv("PATH", [qbs.getEnv("PATH"),
                                            product.moduleProperty("Qt.core", "binPath")]
                                   .join(qbs.pathListSeparator));
                    process.exec(project.qbsFilePath, ["help"], true);
                    var output = new TextFile("output.txt", TextFile.WriteOnly);
                    output.writeLine(process.exitCode());
                    output.writeLine(process.readLine());
                    process.close();

                    // Asynchronous run, successful.
                    process = new Process();
                    process.setEnv("PATH", [qbs.getEnv("PATH"),
                                            product.moduleProperty("Qt.core", "binPath")]
                                   .join(qbs.pathListSeparator));
                    output.writeLine(process.start(project.qbsFilePath, ["help"]));
                    output.writeLine(process.waitForFinished());
                    output.writeLine(process.exitCode());
                    output.writeLine(process.readLine());
                    process.close();

                    // Asynchronous run, unsuccessful.
                    process = new Process();
                    output.writeLine(process.start("blubb", []));
                    process.close();

                    // TODO: Test all the other Process methods as well.

                    output.close();
                };
                return [cmd];
            }
        }
    }
}
