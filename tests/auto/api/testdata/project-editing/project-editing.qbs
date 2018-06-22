CppApplication {
    Group {
        name: "Existing Group 1"
        files: ["existingfile1.txt"]
    }
    property string aFile: "existingfile2.txt"
    Group {
        name: "Existing Group 2"
        files: product.aFile
    }
    Group {
        name: "Existing Group 3"
        files: {
            var file = "existingfile3.txt";
            return file;
        }
    }
    Group {
        name: "Existing Group 4"
        prefix: "subdir/"
        files: []
    }
    Group {
        name: "Existing Group 5"
        prefix: "blubb"
        files: []
    }
    Group {
        name: "Group with wildcards"
        files: "*.klaus"
    }
    Group {
        name: "Other group with wildcards"
        files: "*.wildcard"
    }
    files: "main.cpp"
}
