Product {
    name: "qbsvariant"
    files: [
        "LICENSE.md",
        "README.md",
        "variant.h",
        "variant.hpp"
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: "."
    }
}
