Product {
    id: baseProduct
    property int nr: theProject.initialNr + 1
    name: "product1_" + nr
    property string productName: baseProduct.name
}
