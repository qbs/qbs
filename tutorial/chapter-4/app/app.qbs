//! [0]
// app/app.qbs

MyApplication {
    Depends { name: "mylib" }
    name: "My Application"
    targetName: "myapp"
    files: "main.c"
}
//! [0]
