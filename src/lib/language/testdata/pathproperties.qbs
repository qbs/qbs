import "subdir/pathproperties_base.qbs" as ProductBase

ProductBase {
    name: "product1"
    property path projectFileDir: "."
    property pathList filesInProjectFileDir: ["./aboutdialog.h", "aboutdialog.cpp"]
    Depends { name: "dummy" }
    dummy.includePaths: ["."]
}
