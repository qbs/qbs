import qbs.base 1.0

Project {
    Product {
        type: "application"
        name: "lots.of.dots"

//        Depends { name: "qt.core" }
        Depends { name: "qt.gui" }

        files : [
            "m.a.i.n.cpp",
            "object.narf.h",
            "object.narf.cpp",
            "polka.dots.qrc",
            "dotty.matrix.ui"
        ]
    }
}

