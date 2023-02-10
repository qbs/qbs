Project {
    Product {
        name: "low"
        Export { property string prop: "low"; property string prop2: "low" }
    }
    Product {
        name: "higher1"
        Export { Depends { name: "low" } low.prop: "higher1" }
    }
    Product {
        name: "higher2"
        Export { Depends { name: "low" } low.prop: "higher2" }
    }
    Product {
        name: "highest1"
        Export {
            Depends { name: "low" }
            Depends { name: "higher1" }
            Depends { name: "higher2" }
            low.prop: "highest1"
            low.prop2: "highest"
        }
    }
    Product {
        name: "highest2"
        Export {
            Depends { name: "low" }
            Depends { name: "higher1" }
            Depends { name: "higher2" }
            low.prop: "highest2"
            low.prop2: "highest"
        }
    }
    Product {
        name: "toplevel"
        Depends { name: "highest1" }
        Depends { name: "highest2" }
        low.prop: name
        property bool dummy: { console.info("final prop value: " + low.prop); }
    }
}
