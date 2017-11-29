Product {
    Probe {
        id: dummy
        property stringList l
        configure: {
            l = ["a", undefined, "b"]
        }
    }
}
