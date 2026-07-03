Product {
    Depends { name: "dummy" }

    FileTagger {
        patterns: "*.txt"
        fileTags: "text"
    }
    Group {
        fileTagsFilter: "text"
        fileTags: "tagFromFilter"
        dummy.someString: "valueFromFilter"
    }
    Group {
        name: "the group"
        files: "dummy.txt"
        dummy.zort: "valueFromGroup"
    }
}
