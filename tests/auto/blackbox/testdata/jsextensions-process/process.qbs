import qbs.Environment
import qbs.FileInfo
import qbs.Process
import qbs.TextFile

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        Depends { name: "cpp" }
        type: ["dummy"]
        name: "dummy"
        files: ["main.cpp"]
        Rule {
            multiplex: true
            inputs: ["application"]
            outputFileTags: "dummy"
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

                    // readLine and atEnd
                    var testReadlineFile = new TextFile("123.txt", TextFile.WriteOnly);
                    testReadlineFile.writeLine("1");
                    testReadlineFile.writeLine("2");
                    testReadlineFile.writeLine("3");
                    testReadlineFile.close();

                    process = new Process();
                    if (product.qbs.hostOS.contains("windows"))
                        process.exec(product.qbs.windowsShellPath,
                                     ["/C", "type", "123.txt"],
                                     true);
                    else
                        process.exec("cat", ["123.txt"], true);

                    while(!process.atEnd())
                        output.write(process.readLine());

                    // TODO: Test all the other Process methods as well.

                    output.close();
                };
                return [cmd];
            }
        }
    }
}
