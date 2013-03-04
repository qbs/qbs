import qbs.base 1.0
import "idusagebase.qbs" as DerivedProduct

Project {
    id: theProject
    property int initialNr: 0
    DerivedProduct {
        id: product1
    }
    Product {
        id: product2
        property int nr: theProject.initialNr + product1.nr + 1
        name: "product2_" + nr
    }
    Product {
        id: product3
        property int nr: product2.nr + 1
        name: "product3_" + nr
    }
}
