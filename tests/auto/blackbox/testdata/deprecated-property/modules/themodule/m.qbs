import qbs.Environment

Module {
    property bool newProp
    property bool expiringProp
    property bool forgottenProp

    PropertyOptions {
        name: "newProp"
        description: "Use this, it's good!"
    }
    PropertyOptions {
        name: "expiringProp"
        description: "Use newProp instead."
        removalVersion: Environment.getEnv("REMOVAL_VERSION")
    }
    PropertyOptions {
        name: "veryOldProp"
        description: "Use newProp instead."
        removalVersion: "1.3"
    }
    PropertyOptions {
        name: "forgottenProp"
        description: "Use newProp instead."
        removalVersion: "1.8"
    }
}
