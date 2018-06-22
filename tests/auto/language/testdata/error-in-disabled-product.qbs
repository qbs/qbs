Project {
    Product {
        name: "a"
        condition: false
        property stringList l: [undefined]
    }

    Product {
        name: "b"
        condition: false
        Group {
            name: { throw "boo!" }
        }
    }

    Product {
        name: "c"
        Group {
            condition: false
            name: { throw "boo!" }
        }
    }

    Project {
        condition: false

        Project {
            condition: true
            Product {
                name: "d"
                condition: { throw "ouch!" }
            }
        }
    }

    Product {
        condition: false
        Rule {
            inputs: [5]
        }
    }

    Project {
        condition: false
        minimumQbsVersion: false
    }

    Product {
        name: "e"
        condition: dummy.falseProperty
        Depends { name: "does.not.exist" }
        Depends { name: "dummy" }
    }
}
