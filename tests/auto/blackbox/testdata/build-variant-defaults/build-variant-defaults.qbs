CppApplication {
    property bool validate: {
        var valid = true;
        if (qbs.buildVariant === "release") {
            valid = !qbs.enableDebugCode && !qbs.debugInformation && qbs.optimization === "fast";
        } else if (qbs.buildVariant === "debug") {
            valid = qbs.enableDebugCode && qbs.debugInformation && qbs.optimization === "none";
        } else if (qbs.buildVariant === "profiling") {
            valid = !qbs.enableDebugCode && qbs.debugInformation && qbs.optimization === "fast";
        }

        if (!valid)
            throw "Invalid defaults";
        return valid;
    }
}
