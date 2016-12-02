import qbs

Module {
    property bool newProp
    property bool oldProp
    property bool veryOldProp

    PropertyOptions {
        name: "newProp"
        description: "Use this, it's good!"
    }
    PropertyOptions {
        name: "oldProp"
        description: "Use newProp instead."
        removalVersion: "99.9"
    }
    PropertyOptions {
        name: "veryOldProp"
        description: "Use newProp instead."
        removalVersion: "1.3"
    }
}
