import qbs.Environment

Module {
    setupRunEnvironment: { Environment.putEnv("MY_MODULE", 1); }
}
