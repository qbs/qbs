Project {
    condition: qbs.targetOS.contains("whatever")

    Product {
        name: "never reached"
    }
}
