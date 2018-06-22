Product {
    type: ["text"]
    Depends { name: "texttemplate" }
    texttemplate.dict: ({ wat: "room" })    // typo in key name
    files: [ "boom.txt.in" ]
}
