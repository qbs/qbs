Project {
    Product {
        name: "no_modules"
        property var foo
    }
    Product {
        name: "qt_core"
        dummyqt.core.version: "1.2.3"
        property var foo: dummyqt.core.coreVersion
        Depends {
            name: "dummyqt.core"
        }
    }
    Product {
        name: "qt_gui"
        property var foo: dummyqt.gui.guiProperty
        Depends {
            name: "dummyqt.gui"
        }
    }
    Product {
        name: "qt_gui_network"
        property var foo: dummyqt.gui.guiProperty + ',' + dummyqt.network.networkProperty
        Depends {
            name: "dummyqt"
            submodules: ["gui", "network"]
        }
    }
    Product {
        name: "dummy_twice"
        Depends { name: "dummy" }
        Depends { name: "dummy" }
    }
}
