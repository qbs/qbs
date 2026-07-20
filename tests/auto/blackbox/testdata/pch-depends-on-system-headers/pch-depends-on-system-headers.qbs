CppApplication {
    // extra.h is reachable only through a system include path, so qbs tracks it
    // as a dependency of the PCH only when pchDependsOnSystemHeaders is enabled.
    cpp.systemIncludePaths: [sourceDirectory + "/sysheaders"]
    files: ["main.cpp"]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }
}
