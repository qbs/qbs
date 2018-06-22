import qbs.Probes

Module {
    property bool alt: false

    Probe {
        id: foo
        property string baz
        property bool useAlt: alt
        property string named: product.name
        configure: {
            console.info("running probe " + named);
            baz = useAlt ? "hahaha" : "hello";
            found = true;
        }
    }

    property string something: foo.baz
}
