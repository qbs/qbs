Module {
    Depends { name: "gmod2" }
    Depends { name: "gmod3" }
    property string gmod4_string: "gmod4_string_proto"
    gmod2.gmod2_list: [gmod4_string + "_" + gmod3.gmod3_string]
}
