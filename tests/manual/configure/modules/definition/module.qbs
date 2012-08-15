import qbs.base 1.0

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    Probe {
        id: node
        property string result
        configure: {
            var cmd = '';
            if (qbs.targetOS === "windows")
                cmd = 'cmd /c /date /t';
            else
                cmd = 'date';
            var p = new Process();
            if (0 === p.exec(cmd, [])) {
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
