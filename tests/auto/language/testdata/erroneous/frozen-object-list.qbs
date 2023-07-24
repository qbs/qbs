
Product {
    Probe {
        id: probe
        property varList output
        configure: {
            output = [{"key": "value"}];
            found = true;
        }
    }

    property var test: {
        var result = probe.output;
        result.push({});
        return result;
    }
}
