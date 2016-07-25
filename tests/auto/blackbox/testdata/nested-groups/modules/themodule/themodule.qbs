import qbs

Module {
    Group {
        cpp.defines: ["REQUIRED_FOR_FILE3"]
        Group {
            files: ["file3.cpp", "file3.h"]
        }
    }
}
