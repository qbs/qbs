CppApplication {
    Depends {
        name: "nosuchmodule"
        required: false
    }
    Properties {
        condition: nosuchmodule.present
        files: "main.cpp"
    }
}
