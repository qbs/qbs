import qbs 1.0

Project {
    Product {
        name: "grouptest"

        Depends { name: "gmod.gmod1" }
        Depends { name: "gmod3" }
        Depends { name: "gmod4" }

        gmod.gmod1.gmod1_list2: base.concat([name, gmod.gmod1.gmod1_string])
        gmod.gmod1.gmod1_list3: ["product"]
        gmod.gmod1.p1: 1

        // TODO: Also test nested groups in >= 1.7
        Group  {
            name: "g1"
            files: ["Banana"]

            gmod.gmod1.gmod1_string: name
            gmod.gmod1.gmod1_list2: outer.concat([name])
            gmod.gmod1.p2: 2
            gmod2.prop: 1
            gmod2.commonName: "g1"
            gmod3.gmod3_string: "g1_gmod3"
            gmod4.gmod4_string: "g1_gmod4"
        }

        Group  {
            name: "g2"
            files: ["zort"]

            gmod.gmod1.gmod1_string: name
            gmod.gmod1.p1: 2
            gmod.gmod1.p2: 4
            gmod2.prop: 2
            gmod3.gmod3_string: name + "_gmod3"
            gmod4.gmod4_string: name + "_gmod4"
        }
    }
}
