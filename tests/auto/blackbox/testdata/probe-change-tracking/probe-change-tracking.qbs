Project {
    Probe {
        id: tlpProbe
        property int confValue
        configure: {
            console.info("running tlpProbe");
            confValue = 5;
        }
    }
    property int tlpCount: tlpProbe.confValue

    Project {
        Probe {
            id: subProbe
            property int confValue
            configure: {
                console.info("running subProbe");
                confValue = 7;
            }
        }
        property int subCount: subProbe.confValue

        Product {
            name: "theProduct"
            property bool runProbe
            property int v1: project.tlpCount
            property int v2: project.subCount

            Probe {
                id: productProbe
                condition: product.runProbe
                property int v1: product.v1
                property int v2: product.v2
                configure: {
                    console.info("running productProbe: " + (v1 + v2));
                }
            }
        }
    }
}

