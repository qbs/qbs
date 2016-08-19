import qbs 1.0

Project {
    Product {
        name: "grouptest"
        Depends { name: "dummyqt.core" }
        Group  {
            name: "thegroup"
            files: ["Banana"]
            dummyqt.core.zort: "X"
        }
    }
}
