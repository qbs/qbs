import "../../TheScanner.qbs" as TheScanner

Module {
    Group {
        condition: product.enableGroup
        TheScanner { condition: product.moduleScanner }
    }
}
