Module {
    property bool doFail
    Depends { name: "cpp" }
    cpp.libraries: ["nosuchlib"]
    validate: {
        if (doFail)
            throw "Failure!";
    }
}
