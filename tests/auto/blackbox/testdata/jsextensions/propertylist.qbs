import qbs
import qbs.Process
import qbs.PropertyList
import qbs.TextFile

Product {
    type: {
        var plistobj = new PropertyList();
        if (!plistobj.isEmpty()) {
            throw "newly created PropertyList was not empty!";
        }

        if (plistobj.format() !== undefined) {
            throw "newly created PropertyList did not have an undefined format";
        }

        plistobj.clear();

        if (!plistobj.isEmpty() || plistobj.format() !== undefined) {
            throw "clear() somehow had an effect on an empty PropertyList";
        }

        plistobj.readFromString('{"key":["value"]}');
        if (plistobj.isEmpty() || plistobj.format() !== "json") {
            throw "readFromString did not set format to JSON or object thinks it is empty";
        }

        plistobj.clear();

        if (!plistobj.isEmpty() || plistobj.format() !== undefined) {
            throw "clear() had no effect on a non-empty PropertyList";
        }

        var obj = {
            "Array": ["ListItem1", "ListItem2", "ListItem3"],
            "Integer": 1,
            "Boolean": true,
            "String": "otherString"
        };

        var infoplist = new TextFile("test.xml", TextFile.WriteOnly);
        infoplist.write(JSON.stringify(obj));
        infoplist.close();

        var process = new Process();
        process.exec("plutil", ["-convert", "xml1", "test.xml"]);
        process.close();

        var xmlfile = new TextFile("test.xml", TextFile.ReadOnly);
        var propertyList = new PropertyList();
        propertyList.readFromString(xmlfile.readAll());
        xmlfile.close();

        var jsontextfile = new TextFile("test.json", TextFile.WriteOnly);
        jsontextfile.write(propertyList.toJSONString());
        jsontextfile.close();

        process = new Process();
        process.exec("plutil", ["-convert", "json", "test.xml"]);
        process.close();

        return "Pineapple Steve";
    }
}
