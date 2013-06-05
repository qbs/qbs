import qbs 1.0

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    Probe {
        id: node
        property string result
        configure: {
            var cmd;
            var args;
            if (qbs.targetOS.contains("windows")) {
                cmd = "cmd";
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
        }
    }
    cpp.defines: node.found ? 'TEXT="Configured at ' + node.result + '"' : undefined
}
