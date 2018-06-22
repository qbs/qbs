Project {
    FileTagger {
        patterns: "*.cpp"
        fileTags: ["cpp"]
    }

    Product {
        name: "filetagger_project_scope"
        files: ["main.cpp"]
    }

    Product {
        name: "filetagger_product_scope"
        files: ["drawline.asm"]
        FileTagger {
            patterns: "*.asm"
            fileTags: ["asm"]
        }
    }

    Product {
        name: "filetagger_static_pattern"
        files: "Banana"
        FileTagger {
            patterns: "Banana"
            fileTags: ["yellow"]
        }
    }

    Product {
        name: "unknown_file_tag"
        files: "narf.zort"
    }

    Product {
        name: "set_file_tag_via_group"
        Group {
            files: ["main.cpp"]
            fileTags: ["c++"]
        }
    }

    Product {
        name: "override_file_tag_via_group"
        Group {
            files: "main.cpp"   // gets file tag "cpp" through the FileTagger
            fileTags: ["c++"]
        }
    }

    Product {
        name: "add_file_tag_via_group"
        Group {
            overrideTags: false
            files: "main.cpp"
            fileTags: ["zzz"]
        }
    }

    Product {
        name: "prioritized_filetagger"
        files: ["main.cpp"]
        FileTagger {
            patterns: ["*.cpp"]
            fileTags: ["cpp1"]
            priority: 3
        }
        FileTagger {
            patterns: ["*.cpp"]
            fileTags: ["cpp2"]
            priority: 3
        }
        FileTagger {
            patterns: ["*.cpp"]
            fileTags: ["ignored"]
            priority: 2
        }
    }
}
