import qbs 1.0

Project {
    Product {
        name: "product1"
        condition: false
        someNonsense: "Bitte stellen Sie die Tassen auf den Tisch."
    }
    Product {
        name: "product2"
        Group {
            condition: false
            moreNonsense: "Follen. Follen. Hünuntergefollen. Auf dön Töppüch."
        }
    }
}
