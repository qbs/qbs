import qbs.Environment

Product {
    Probe {
        id: dummy
        property var env: Environment.currentEnv()
        configure: {
            console.info(JSON.stringify(env));
        }
    }
}
