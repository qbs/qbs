import qbs.Probes

Project {
    Product {
        name: "product_no_condition"
    }
    Product {
        name: "product_true_condition"
        condition: 1 === 1
    }
    Product {
        name: "product_false_condition"
        condition: 1 === 2
    }
    Product {
        name: "product_condition_dependent_of_module"
        condition: qbs.architecture !== (qbs.architecture + "foo")
    }
    Product {
        name: "product_probe_condition_true"
        condition: trueProbe.found
        Probe {
            id: trueProbe
            configure: { found = true; }
        }
    }
    Product {
        name: "product_probe_condition_false"
        condition: falseProbe.found
        Probe {
            id: falseProbe
            configure: { found = false; }
        }
    }
}
