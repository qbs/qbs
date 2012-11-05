import "jsimportsinmultiplescopes.js" as MyFunctions

Product {
    name: MyFunctions.getName(qbs)
    qbs.installPrefix: MyFunctions.getInstallPrefix()
    files: "main.cpp"
}
