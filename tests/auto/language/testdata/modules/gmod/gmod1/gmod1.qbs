Module {
    Depends { name: "gmod2" }
    Depends { name: "gmod4" }
    property stringList gmod1_list1: ["gmod1_list1_proto", gmod1_string]
    property stringList gmod1_list2: ["gmod1_list2_proto"]
    property stringList gmod1_list3: [gmod1_string]
    property string gmod1_string: "gmod1_string_proto"
    property string commonName: "commonName_in_gmod1"
    property int depProp: gmod2.prop
    property int p0: p1 + p2
    property int p1: 0
    property int p2: 0

    gmod2.gmod2_string: gmod1_string
    gmod2.gmod2_list: [gmod1_string, commonName]
}
