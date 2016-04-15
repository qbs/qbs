import qbs

Product {
    name: "theProduct"
    property bool enableProbe
    Probe {
        condition: enableProbe
        configure: {
            throw "Error!";
        }
    }
}
