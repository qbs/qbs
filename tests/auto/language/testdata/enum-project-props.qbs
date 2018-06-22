Project {
    property string anExistingFile: "dummy.txt"
    Product {
        files: {
            for (var k in project) {
                if (k === "anExistingFile")
                    return [project[k]];
            }
            return [];
        }
    }
}
