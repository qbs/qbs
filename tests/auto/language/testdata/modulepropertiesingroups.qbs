Project {
    Product {
        name: "grouptest"

        Depends { name: "gmod.gmod1" }
        Depends { name: "gmod3" }
        Depends { name: "gmod4" }

        gmod.gmod1.gmod1_list2: base.concat([name, gmod.gmod1.gmod1_string])
        gmod.gmod1.gmod1_list3: ["product"]
        gmod.gmod1.p1: 1

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

            Group {
                name: "g1.1"

                gmod.gmod1.gmod1_string: name
                gmod.gmod1.gmod1_list2: outer.concat([name])
                gmod.gmod1.p2: 4
                gmod2.prop: 2
                gmod2.commonName: name
                gmod3.gmod3_string: "g1.1_gmod3"
                gmod4.gmod4_string: "g1.1_gmod4"
            }

            Group {
                name: "g1.2"

                gmod.gmod1.gmod1_string: name
                gmod.gmod1.gmod1_list2: outer.concat([name])
                gmod.gmod1.p2: 8
                gmod2.commonName: name
                gmod3.gmod3_string: "g1.2_gmod3"
            }
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

            Group {
                name: "g2.1"

                Group {
                    name: "g2.1.1"

                    gmod.gmod1.gmod1_list2: [name]
                    gmod.gmod1.p2: 15
                }
            }
        }
    }
    Product {
        name: "grouptest2"
        Depends { name: "gmod.gmod1" }
        Group {
            name: "g1"
            gmod.gmod1.gmod1_list2: ["G1"]
            Group {
                name: "g1.1"
                gmod.gmod1.gmod1_string: "G1.1"
            }
        }
    }
}
