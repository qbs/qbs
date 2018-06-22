Project {
    property string productName: "p1"
    Product { name: project.productName }
    references: ["referenced-file-errors.qbs"]
}
