Module {
    property bool doFail
    Depends { name: "cpp" }
    cpp.dynamicLibraries: ["nosuchlib"]
    validate: {
        if (doFail)
            throw "Failure!";
    }
}
