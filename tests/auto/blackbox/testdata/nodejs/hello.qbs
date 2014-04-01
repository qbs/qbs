import qbs

NodeJSApplication {
    nodejs.applicationFile: "hello.js"
    name: "hello"
    files: "hello.js"
}
