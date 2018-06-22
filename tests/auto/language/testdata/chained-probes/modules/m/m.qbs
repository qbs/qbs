Module {
    Probe {
        id: probe1
        property string probe1Prop
        configure: { probe1Prop = "probe1Val"; found = true; }
    }
    Probe {
        id: probe2
        property string inputProp: prop1
        property string probe2Prop
        configure: { probe2Prop = inputProp + "probe2Val"; found = true; }
    }
    property string prop1: probe1.probe1Prop
    property string prop2: probe2.probe2Prop
}
