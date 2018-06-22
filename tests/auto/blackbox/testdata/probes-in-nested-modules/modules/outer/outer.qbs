Module {
    Depends { name: "inner" }

    Probe {
        id: foo2
        property string barz
        property string named: product.name
        configure: {
            console.info("running second probe " + named);
            barz = "goodbye";
            found = true;
        }
    }

    property string something: inner.something
    property string somethingElse: foo2.barz
}
