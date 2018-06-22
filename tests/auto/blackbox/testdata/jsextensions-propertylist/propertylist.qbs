import qbs.Process
import qbs.PropertyList
import qbs.TextFile

Product {
    type: ["Pineapple Steve"]
    property bool dummy: {
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
        jsontextfile.write(propertyList.toJSON());
        jsontextfile.close();

        propertyList.writeToFile("test2.json", "json-compact");
        propertyList.writeToFile("test3.json", "json-pretty");

        process = new Process();
        process.exec("plutil", ["-convert", "json", "test.xml"]);
        process.close();

        propertyList = new PropertyList();
        propertyList.readFromFile("test.xml");
        if (propertyList.format() !== "json") { // yes, JSON -- ignore the file extension
            throw "expected property list format json but got " + propertyList.format();
        }

        if (propertyList.isEmpty()) {
            throw "PropertyList was 'empty' after being loaded with data";
        }

        var opensteptextfile = new TextFile("test.openstep.plist", TextFile.WriteOnly);
        opensteptextfile.write('{ rootObject = ( "val1", "val3", "val5", /* comment */ "val7", "val9", ); }');
        opensteptextfile.close();

        propertyList = new PropertyList();
        propertyList.readFromFile("test.openstep.plist");
        if (propertyList.format() !== "openstep") {
            throw "expected property list format openstep but got " + propertyList.format();
        }

        var jsonObj = JSON.parse(propertyList.toJSON());
        if (jsonObj["rootObject"].length != 5) {
            throw "going from OpenStep to a JSON string to a JSON object somehow broke";
        }

        propertyList.clear();
        propertyList.readFromString('<dict><key>foo</key><string>barz</string></dict>');
        jsonObj = JSON.parse(propertyList.toJSON());
        if (jsonObj["foo"] !== "barz") {
            throw "the XML plist did not get parsed properly";
        }

        propertyList.writeToFile("woof.xml", "xml1");
        propertyList.readFromFile("woof.xml");
        if (propertyList.format() !== "xml1") {
            throw "round trip writing and reading XML failed";
        }

        propertyList.writeToFile("woof.plist", "binary1");
        propertyList.readFromFile("woof.plist");
        if (propertyList.format() !== "binary1") {
            throw "round trip writing and reading binary failed";
        }

        if (jsonObj["foo"] !== "barz") {
            throw "the binary plist did not get parsed properly";
        }

        if (propertyList.toString("json") !== propertyList.toString("json-compact") ||
            propertyList.toJSON() !== propertyList.toJSON("compact")) {
            throw "json and json-compact formats were not equivalent";
        }

        if (propertyList.toString("json") === propertyList.toString("json-pretty") ||
            propertyList.toJSON() === propertyList.toJSON("pretty")) {
            throw "json and json-pretty formats were not different";
        }

        if (propertyList.toString("xml1") !== propertyList.toXMLString()) {
            throw 'toString("xml1") and toXMLString() were not equivalent';
        }

        return true;
    }
}
