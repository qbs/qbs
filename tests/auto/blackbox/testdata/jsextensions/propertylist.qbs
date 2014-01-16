import qbs
import qbs.Process
import qbs.PropertyList
import qbs.TextFile

Product {
    type: {
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
        propertyList.read(xmlfile.readAll());
        xmlfile.close();

        var jsontextfile = new TextFile("test.json", TextFile.WriteOnly);
        jsontextfile.write(propertyList.toJSON());
        jsontextfile.close();

        process = new Process();
        process.exec("plutil", ["-convert", "json", "test.xml"]);
        process.close();

        return "Pineapple Steve";
    }
}
