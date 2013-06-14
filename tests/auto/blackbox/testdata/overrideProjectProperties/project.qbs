import qbs 1.0

Project {
    property string nameSuffix: ""
    property bool someBool
    property int someInt
    property stringList someStringList
    Product {
        type: "application"
        property string mainFile: ""
        name: "MyApp" + nameSuffix
        Depends { name: "cpp" }
        files: {
            // Check types of the project's custom properties here.
            // Provoke a build error if the expected types do not match.
            var wrongFile = "doesnotexist.cpp";
            if (typeof project.someBool != "boolean") {
                print("someBool has a wrong type: " + typeof project.someBool);
                return wrongFile;
            }
            if (typeof project.someInt != "number") {
                print("someInt has a wrong type: " + typeof project.someInt);
                return wrongFile;
            }
            if (typeof project.someStringList != "object") {
                print("someStringList has a wrong type: " + typeof project.someStringList);
                return wrongFile;
            }

            // Return the mainFile property that is set on the command line.
            return [mainFile];
        }
    }
}
