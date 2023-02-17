Project {
    condition: qbs.targetOS.includes("whatever")

    Product {
        name: "never reached"
    }
}
