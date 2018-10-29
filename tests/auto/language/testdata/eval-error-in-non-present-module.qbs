import qbs

Product {
    name: "p"
    property bool moduleRequired
    Depends { name: "broken"; required: moduleRequired }
}
