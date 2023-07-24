
Product {
    Probe {
        id: probe
        property var output
        configure: {
            output = {"key": "value"}
            found = true
        }
    }

    property var test: {
        "use strict"
        var result = probe.output;
        result.key = "newValue";
        return result;
    }
}
