QtApplication {
    Probe {
        id: qtConfigProbe
        property stringList moduleConfig: Qt.core.moduleConfig
        configure: {
            console.info("Qt requires gold: " + moduleConfig.includes("use_gold_linker"));
        }
    }
    files: "main.cpp"
}
