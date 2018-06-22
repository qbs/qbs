CppApplication {
    name: "test2"
    type: base.concat("autotest")

    Depends { name: "autotest" }
    autotest.workingDir: sourceDirectory

    files: "test2.cpp"
}
