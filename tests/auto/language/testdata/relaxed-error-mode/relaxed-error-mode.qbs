Project {
    Product {
        name: "recursive depender"
        Depends { name: "depender required" }
        files: "file1.txt"
    }
    Product {
        name: "broken"
        Depends { name: "nosuchmodule" }
    }
    Product {
        name: "depender required"
        Depends { name: "broken" }
        files: "file1.txt"
    }
    Product {
        name: "depender nonrequired"
        Depends { name: "broken"; required: false }
        files: "file1.txt"
    }
    Product {
        name: "missing file"
        files: ["file1.txt", "file3.txt", "file2.txt"]
    }
    Product {
        name: "fine"
        files: "file2.txt"
    }
}
