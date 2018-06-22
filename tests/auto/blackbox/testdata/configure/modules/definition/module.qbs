import qbs.Process

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    Probe {
        id: node
        property stringList targetOS: qbs.targetOS
        property stringList windowsShellPath: qbs.windowsShellPath
        property string result
        configure: {
            var cmd;
            var args;
            var p = path;
            if (targetOS.contains("windows")) {
                cmd = windowsShellPath;
                args = ["/c", "date", "/t"];
            } else {
                cmd = 'date';
                args = [];
            }
            var p = new Process();
            if (0 === p.exec(cmd, args)) {
                found = true;
                result = p.readLine();
            } else {
                found = false;
                result = undefined;
            }
            p.close();
        }
    }
    cpp.defines: node.found ? 'TEXT="Configured at ' + node.result + '"' : undefined
}
