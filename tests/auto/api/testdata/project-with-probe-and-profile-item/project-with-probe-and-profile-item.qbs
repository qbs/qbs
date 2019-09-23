Project {

    property bool probesEvaluated: probe.found

    Probe {
        id: probe
        configure: {
            found = true;
        }
    }

    Profile {
        name: "the-profile"
        cpp.includePaths: {
            if (!probesEvaluated)
                throw "project-level probes not evaluated";
        }
    }
}
