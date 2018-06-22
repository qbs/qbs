Project {
    property bool someTrueProperty: true
    Product {
        name: "no_condition_no_group"
        files: ["main.cpp"]
    }
    Product {
        name: "no_condition"
        Group {
            files: ["main.cpp"]
        }
    }
    Product {
        name: "true_condition"
        Group {
            condition: true
            files: ["main.cpp"]
        }
    }
    Product {
        name: "false_condition"
        Group {
            condition: false
            files: ["main.cpp"]
        }
    }
    Product {
        name: "true_condition_from_product"
        property bool anotherTrueProperty: true
        Group {
            condition: anotherTrueProperty
            files: ["main.cpp"]
        }
    }
    Product {
        name: "true_condition_from_project"
        Group {
            condition: project.someTrueProperty
            files: ["main.cpp"]
        }
    }

    Product {
        name: "condition_accessing_module_property"
        Group {
            condition: qbs.targetOS.contains("narf")
            files: ["main.cpp"]
            qbs.install: false
        }
    }
}
