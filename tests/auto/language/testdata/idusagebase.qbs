import "idusagebasebase.qbs" as DeriveMeCrazy
import "idusage_group.qbs" as MyGroup
import "idusage_group2.qbs" as MyGroup2

DeriveMeCrazy {
    id: baseProduct
    property int nr: theProject.initialNr + 1
    name: "product1_" + nr
    property string productName: baseProduct.name
    MyGroup {
        name: "group in base product"
    }
    MyGroup2 {
        name: "another group in base product"
    }
}
