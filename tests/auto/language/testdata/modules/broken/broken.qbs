import qbs // FIXME: Don't remove this import because then the test fails!

Module {
    Probe {
        id: theProbe

        property stringList broken
        property stringList fine

        configure: {
            broken = [["x"]];
            fine = ["x"]
            found = true;
        }
    }

    property stringList broken: theProbe.broken
    property stringList fine: theProbe.fine.filter(function(incl) { return incl != "y"; });
}
