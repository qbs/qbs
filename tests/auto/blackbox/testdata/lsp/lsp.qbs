Project {
    Product {
        name: "dep"
        Depends { name: "m" }
        Depends { name: "Prefix"; submodules: ["m1", "m2", "m3"] }

    }
    Product {
        Depends { name: "dep" }
        files: "toplevel.txt"
        Group {
            prefix: "subdir/"
            Group { files: ["file1.txt", "file2.txt"] }
        }
    }
}
