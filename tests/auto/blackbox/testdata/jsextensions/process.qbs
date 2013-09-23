import qbs
import qbs.Process
import qbs.TextFile

Project {
    property path qbsFilePath
    Product {
        type: {
            // Synchronous run, successful.
            var process = new Process();
            process.exec(qbsFilePath, ["help"], true);
            var output = new TextFile("output.txt", TextFile.WriteOnly);
            output.writeLine(process.exitCode());
            output.writeLine(process.readLine());
            process.close();

            // Asynchronous run, successful.
            process = new Process();
            output.writeLine(process.start(qbsFilePath, ["help"]));
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
            return "My hovercraft is full of eels.";
        }
    }
}
