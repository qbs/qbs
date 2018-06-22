Module {
    Group {
        cpp.defines: ["REQUIRED_FOR_FILE3"]
        Group {
            prefix: product.sourceDirectory + '/'
            files: ["file3.cpp", "file3.h"]
        }
    }
}
