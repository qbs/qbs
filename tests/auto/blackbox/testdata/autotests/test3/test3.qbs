CppApplication {
    name: "test3"
    type: base.concat("autotest")

    Depends { name: "autotest" }
    autotest.allowFailure: true

    files: "test3.cpp"
}
