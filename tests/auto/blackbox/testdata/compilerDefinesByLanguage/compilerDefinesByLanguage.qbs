Project {
    CppDefinesApp {
        name: "A"
        // eqiuvalent to ["c"] since we always need some compiler defines
        // for the architecture detection, etc.
        cpp.enableCompilerDefinesByLanguage: []
        property var foo: {
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (!cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "B"
        cpp.enableCompilerDefinesByLanguage: ["cpp"]
        property var foo: {
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (!cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "C"
        condition: enableObjectiveC
        cpp.enableCompilerDefinesByLanguage: ["objc"]
        property var foo: {
            if (!enableObjectiveC)
                return;
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (!cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "D"
        condition: enableObjectiveC
        cpp.enableCompilerDefinesByLanguage: ["objcpp"]
        property var foo: {
            if (!enableObjectiveC)
                return;
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (!cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "E"
        condition: enableObjectiveC
        cpp.enableCompilerDefinesByLanguage: ["cpp", "objcpp"]
        property var foo: {
            if (!enableObjectiveC)
                return;
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (!cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (!cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "F"
        cpp.enableCompilerDefinesByLanguage: ["c", "cpp"]
        property var foo: {
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (!cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (!cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }

    CppDefinesApp {
        name: "G"
        condition: enableObjectiveC
        cpp.enableCompilerDefinesByLanguage: ["objc", "objcpp"]
        property var foo: {
            if (!enableObjectiveC)
                return;
            if (!cpp.compilerDefinesByLanguage)
                throw "ASSERT cpp.compilerDefinesByLanguage: "
                        + cpp.compilerDefinesByLanguage;
            if (cpp.compilerDefinesByLanguage["c"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"c\"]: "
                        + cpp.compilerDefinesByLanguage["c"];
            if (cpp.compilerDefinesByLanguage["cpp"])
                throw "ASSERT !cpp.compilerDefinesByLanguage[\"cpp\"]: "
                        + cpp.compilerDefinesByLanguage["cpp"];
            if (!cpp.compilerDefinesByLanguage["objc"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"objc\"]: "
                        + cpp.compilerDefinesByLanguage["objc"];
            if (!cpp.compilerDefinesByLanguage["objcpp"])
                throw "ASSERT cpp.compilerDefinesByLanguage[\"objcpp\"]: "
                        + cpp.compilerDefinesByLanguage["objcpp"];
        }
    }
}
