import qbs.Environment

Module {
    property string varPrefix: "BUILD_ENV_"
    setupBuildEnvironment: {
        Environment.putEnv(product.buildenv.varPrefix + product.name.toUpperCase(), "1");
    }
}
