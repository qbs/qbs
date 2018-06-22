Project {
    Product {
        name: "a"
        Depends { name: "outer" }
        inner.alt: true
        property bool dummy: {
            console.info("product " + name + ", inner.something = " + inner.something);
            console.info("product " + name + ", outer.something = " + outer.something);
            console.info("product " + name + ", outer.somethingElse = " + outer.somethingElse);
            return true;
        }
        type: ["foo"]
    }

    Product {
        name: "b"
        Depends { name: "inner" }
        inner.alt: true
        property bool dummy: {
            console.info("product " + name + ", inner.something = " + inner.something);
            return true;
        }
        type: ["foo"]
    }

    Product {
        name: "c"
        Depends { name: "inner" }
        inner.alt: false
        property bool dummy: {
            console.info("product " + name + ", inner.something = " + inner.something);
            return true;
        }
        type: ["foo"]
    }
}
