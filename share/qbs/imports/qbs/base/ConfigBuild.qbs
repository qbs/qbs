Module {
    property string libraryType: "dynamic"
    PropertyOptions {
        name: "libraryType"
        allowedValues: ["static", "dynamic"]
    }
    property string pluginType: libraryType
    PropertyOptions {
        name: "pluginType"
        allowedValues: ["static", "dynamic"]
    }
}
