import qbs 1.0

Project {
    Product {
        name: "grouptest"

        Depends { name: "gmod1" }
        Depends { name: "dummyqt.core" }

        gmod1.listProp2: ["product", gmod1.stringProp]
        gmod1.p1: 1

        // TODO: Also test nested groups in >= 1.7
        Group  {
            name: "g1"
            files: ["Banana"]

            gmod1.stringProp: "g1"
            gmod1.listProp2: ["g1"]
            gmod1.p2: 2
            gmod2.prop: 1
            dummyqt.core.zort: "X"
        }

        Group  {
            name: "g2"
            files: ["zort"]

            gmod1.stringProp: "g2"
            gmod1.listProp2: ["g2"]
            gmod1.p1: 2
            gmod1.p2: 4
            gmod2.prop: 2
        }
    }
}
