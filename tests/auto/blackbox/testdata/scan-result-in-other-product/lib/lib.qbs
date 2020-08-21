Product {
    files: "lib.h"
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: exportingProduct.sourceDirectory
    }
}
