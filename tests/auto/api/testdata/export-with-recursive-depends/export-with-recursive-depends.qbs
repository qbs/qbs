Project {
    CppApplication {
        name: "app1"
        files: "main1.cpp"
        Export { Depends { name: "module1" } }
    }

    CppApplication {
        name: "app2"
        Depends { name: "app1" }
        files: "main2.cpp"
    }
}
