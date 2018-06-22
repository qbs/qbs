import qbs.Probes

CppApplication {
    Probe {
        id: probe1
        property string someString
        configure: {
            someString = "one";
            found = true;
        }
    }
    Probe {
        id: probe2
        configure: {
            found = false;
        }
    }
    type: ["application"]
    name: "MyApp"
    consoleApplication: {
        if (!probe1.found)
            throw "probe1 not found";
        if (probe2.found)
            throw "probe2 unexpectedly found";
        if (probe1.someString !== "one")
            throw "probe1.someString expected to be \"one\"."
        return true
    }
    files: ["main.cpp"]
}

