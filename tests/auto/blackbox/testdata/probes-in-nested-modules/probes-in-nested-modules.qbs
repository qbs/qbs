import qbs

Project {
    Product {
        name: "a"
        Depends { name: "outer" }
        inner.alt: true
        type: {
            print("product " + name + ", inner.something = " + inner.something);
            print("product " + name + ", outer.something = " + outer.something);
            print("product " + name + ", outer.somethingElse = " + outer.somethingElse);
            return ["foo"];
        }
    }

    Product {
        name: "b"
        Depends { name: "inner" }
        inner.alt: true
        type: {
            print("product " + name + ", inner.something = " + inner.something);
            return ["foo"];
        }
    }

    Product {
        name: "c"
        Depends { name: "inner" }
        inner.alt: false
        type: {
            print("product " + name + ", inner.something = " + inner.something);
            return ["foo"];
        }
    }
}
