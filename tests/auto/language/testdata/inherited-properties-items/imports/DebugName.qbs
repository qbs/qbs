import qbs

Properties {
    condition: qbs.buildVariant === "debug"
    name: "product_debug"
}
