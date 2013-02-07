Project {
    Product {
        name: "no_modules"
    }
    Product {
        name: "qt_core"
        Depends {
            name: "dummyqt.core"
        }
    }
    Product {
        name: "qt_gui"
        Depends {
            name: "dummyqt.gui"
        }
    }
    Product {
        name: "qt_gui_network"
        Depends {
            name: "dummyqt"
            submodules: ["gui", "network"]
        }
    }
}
