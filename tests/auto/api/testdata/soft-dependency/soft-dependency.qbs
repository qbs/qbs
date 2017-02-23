import qbs

Application {
    Depends {
        name: "nosuchmodule"
        required: false
    }
    Depends {
        name: "cpp"
        condition: nosuchmodule.present
    }

    files: "main.cpp"
}
