import qbs 1.0

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
        condition: qbs.endianness !== (qbs.endianness + "foo")
    }
}
