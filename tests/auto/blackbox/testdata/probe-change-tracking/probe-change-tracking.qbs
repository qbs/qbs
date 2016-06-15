import qbs

Product {
    name: "theProduct"
    property bool runProbe

    Probe {
        condition: product.runProbe
        configure: {
            console.info("running probe");
        }
    }
}
