Project {
    qbsModuleProviders: "provider_a"
    name: "project"

    Profile {
        name: "profile1"
        qbs.sysroot: "/sysroot1"
    }

    Profile {
        name: "profile2"
        qbs.sysroot: "/sysroot2"
    }

    Product {
        name: "product1"
        Depends { name: "qbsmetatestmodule" }
        property bool dummy: {
            console.info("product1.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
        }
        // multiplex over profiles, sysroot should not be cached
        qbs.profiles: ["profile1", "profile2"]
    }

    Product {
        name: "product2"
        Depends { name: "qbsmetatestmodule" }
        property bool dummy: {
            console.info("product2.qbsmetatestmodule.prop: " + qbsmetatestmodule.prop);
        }
        // multiplex over profiles, sysroot should not be cached
        qbs.profiles: ["profile1", "profile2"]
    }
}
