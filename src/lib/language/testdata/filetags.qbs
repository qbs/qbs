import qbs 1.0

Project {
    FileTagger {
        pattern: "*.cpp"
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
            pattern: "*.asm"
            fileTags: ["asm"]
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
        files: "main.cpp"   // gets file tag "cpp" through the FileTagger
        Group {
            files: product.files
            fileTags: ["c++"]
        }
    }

    Product {
        name: "add_file_tag_via_group"
        files: "main.cpp"
        Group {
            overrideTags: false
            files: "main.cpp"
            fileTags: ["zzz"]
        }
    }

    Product {
        name: "add_file_tag_via_group_and_file_ref"
        files: "main.cpp"
        Group {
            overrideTags: false
            files: product.files
            fileTags: ["zzz"]
        }
    }
}
