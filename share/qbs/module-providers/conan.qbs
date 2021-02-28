import "conan.js" as ConanHelper

ModuleProvider {
    /* input */
    property path installDirectory

    isEager: false

    relativeSearchPaths: {
        return ConanHelper.configure(installDirectory, moduleName, outputBaseDir);
    }
}
