import qbs.BinaryFile

Product {
    type: ["dummy"]
    Rule {
        multiplex: true
        outputFileTags: "dummy"
        prepare: {
            var commands = [];
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var source = new BinaryFile("source.dat", BinaryFile.WriteOnly);
                source.write([ 0x01, 0x02 ]); // First data.
                // Do not close the file to test the auto close functionality.
            };
            commands.push(cmd);
            cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                source = new BinaryFile("source.dat", BinaryFile.ReadWrite);
                source.seek(source.size());
                source.write([ 0x03, 0x04 ]); // Second data.
                source.close();
                source = new BinaryFile("source.dat", BinaryFile.ReadWrite);
                var destination = new BinaryFile("destination.dat", BinaryFile.WriteOnly);
                destination.write(source.atEof() ? [ 0xFF ] : [ 0x00 ]);
                while (true) {
                    var data = source.read(1);
                    if (!data || data.length == 0)
                        break;
                    destination.write(data);
                }
                source.resize(0); // truncate
                destination.write([ 0x05, 0x06 ]); // Third data.
                destination.write(source.atEof() ? [ 0xFF ] : [ 0x00 ]);
                source.close();
                destination.close();
            };
            commands.push(cmd);
            return commands;
        }
    }
}
