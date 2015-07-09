import qbs
import qbs.Probes

Module {
    Probe {
        id: foo
        property string baz
        configure: {
            print("running probe...");
            baz = "hello";
            found = true;
        }
    }

    property string something: foo.baz
}
