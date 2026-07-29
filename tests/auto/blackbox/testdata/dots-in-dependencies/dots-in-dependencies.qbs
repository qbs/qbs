// Regression: include paths / #includes containing ".." must not cause unrelated
// translation units to rebuild when a header changes.
CppApplication {
    consoleApplication: true
    name: "app"
    // Intentionally unclean; equivalent to sourceDirectory + "/include"
    cpp.includePaths: [sourceDirectory, sourceDirectory + "/include/../include"]
    // Headers are intentionally not listed as product files so the scanner records
    // them as FileDependencies (same as external headers in large projects).
    files: ["a.cpp", "b.cpp"]
}
