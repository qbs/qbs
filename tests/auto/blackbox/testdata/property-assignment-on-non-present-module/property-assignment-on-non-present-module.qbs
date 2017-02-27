import qbs

Product {
    Depends { name: "nein"; required: false }
    nein.doch: "ohhh!"
}
