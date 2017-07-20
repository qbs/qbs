import qbs
import qbs.Environment
import qbs.FileInfo
import qbs.Process
import qbs.TextFile

Project {
    Product {
        Depends { name: "cpp" }
        type: ["dummy"]
        name: "dummy"
        files: ["main.cpp"]
        Rule {
            multiplex: true
            inputs: ["application"]
            Artifact {
                filePath: "dummy.txt"
                fileTags: ["dummy"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    var exeFilePath = FileInfo.joinPaths(product.buildDirectory,
                                                         product.cpp.executablePrefix
                                                         + "dummy"
                                                         + product.cpp.executableSuffix);

                    // Synchronous run, successful.
                    var process = new Process();
                    var pathVal = "why, hello!";
                    process.setEnv("SOME_ENV", pathVal);
                    process.exec(exeFilePath, ["help"], true);
                    var output = new TextFile("output.txt", TextFile.WriteOnly);
                    output.writeLine(process.exitCode());
                    output.writeLine(process.readLine());
                    process.close();

                    // Asynchronous run, successful.
                    process = new Process();
                    process.setEnv("SOME_ENV", pathVal);
                    output.writeLine(process.start(exeFilePath, ["help"]));
                    output.writeLine(process.waitForFinished());
                    output.writeLine(process.exitCode());
                    output.writeLine(process.readLine());
                    process.close();

                    // Asynchronous run, unsuccessful.
                    process = new Process();
                    output.writeLine(process.start("blubb", []));
                    process.close();

                    // closeWriteChannel test
                    process = new Process();
                    if (product.qbs.hostOS.contains("windows"))
                        process.start(product.qbs.windowsShellPath,
                            ["/C", product.qbs.windowsSystemRoot + "\\system32\\sort.exe"]);
                    else
                        process.start("cat", []);
                    process.writeLine("should be");
                    process.closeWriteChannel();
                    process.writeLine("should not be");
                    if (!process.waitForFinished())
                        process.kill();
                    output.write(process.readStdOut());
                    process.close();

                    // TODO: Test all the other Process methods as well.

                    output.close();
                };
                return [cmd];
            }
        }
    }
}
