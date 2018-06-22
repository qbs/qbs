Project {
    Application {
        name: "app"
        Depends { name: "dep"; required: false }
        files: "main.cpp"
    }
    Product {
        name: "dep"
        condition: eval(conditionString)
        property string conditionString
        Depends { name: "nosuchmodule"; required: false }
        Depends { name: "broken"; required: false }
        Export {
            Depends { name: "cpp" }
        }
    }
}
