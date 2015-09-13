import qbs

Project {
    Product {
        name: "a"
        Depends { name: "outer" }
        inner.alt: true
        type: {
            console.info("product " + name + ", inner.something = " + inner.something);
            console.info("product " + name + ", outer.something = " + outer.something);
            console.info("product " + name + ", outer.somethingElse = " + outer.somethingElse);
            return ["foo"];
        }
    }

    Product {
        name: "b"
        Depends { name: "inner" }
        inner.alt: true
        type: {
            console.info("product " + name + ", inner.something = " + inner.something);
            return ["foo"];
        }
    }

    Product {
        name: "c"
        Depends { name: "inner" }
        inner.alt: false
        type: {
            console.info("product " + name + ", inner.something = " + inner.something);
            return ["foo"];
        }
    }
}
