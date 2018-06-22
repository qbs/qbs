Product {
    name: "theProduct"
    property bool enableProbe
    Probe {
        id: whatever
        condition: enableProbe
        configure: {
            throw "Error!";
        }
    }
}
