CppApplication {
    name: "test1"
    type: base.concat("autotest")

    Depends { name: "autotest" }
    autotest.arguments: "--dummy"

    files: "test1.cpp"
}
