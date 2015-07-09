import qbs

Project {
    Product {
        name: "a"
        Depends { name: "outer" }
        type: {
            print("product " + name + ", outer.something = " + outer.something);
            return ["foo"];
        }
    }

    Product {
        name: "b"
        Depends { name: "inner" }
        type: {
            print("product " + name + ", inner.something = " + inner.something);
            return ["foo"];
        }
    }
}
