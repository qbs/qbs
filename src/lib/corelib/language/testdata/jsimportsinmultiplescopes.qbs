import "jsimportsinmultiplescopes.js" as MyFunctions

Product {
    name: MyFunctions.getName(qbs)
    qbs.installDir: MyFunctions.getInstallDir()
    files: "main.cpp"
}
