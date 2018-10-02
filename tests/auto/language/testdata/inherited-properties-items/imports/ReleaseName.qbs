import qbs

Properties {
    condition: qbs.buildVariant === "release"
    name: "product_release"
}
