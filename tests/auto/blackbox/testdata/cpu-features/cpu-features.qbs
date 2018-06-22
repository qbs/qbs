CppApplication {
    Depends { name: "cpufeatures" }
    cpufeatures.x86_sse2: true
    cpufeatures.x86_avx: true
    cpufeatures.x86_avx512f: false

    files: ["main.cpp"]
    property bool dummy: {
        console.info("is x86: " + (qbs.architecture === "x86"));
        console.info("is x64: " + (qbs.architecture === "x86_64"));
        console.info("is gcc: " + qbs.toolchain.contains("gcc"));
        console.info("is msvc: " + qbs.toolchain.contains("msvc"));
    }
}
