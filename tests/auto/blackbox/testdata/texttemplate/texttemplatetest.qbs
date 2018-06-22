Product {
    name: "one"
    type: ["text"]
    files: ["output.txt.in"]
    Depends { name: "texttemplate" }
    texttemplate.dict: ({
        foo: "fu",
        bar: "BAR",
        baz: "buzz",
    })
    Group {
        files: ["cdefgabc.txt.in"]
        texttemplate.outputFileName: "lalala.txt"
        texttemplate.dict: ({
            c: "do",
            d: "re",
            e: "mi",
            f: "fa",
            g: "so",
            a: "la",
            b: "ti",
        })
    }
}
