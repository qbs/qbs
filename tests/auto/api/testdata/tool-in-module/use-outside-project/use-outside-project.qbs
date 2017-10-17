import qbs

Product {
    name: "user-outside-project"
    type: ["thetool.output"]
    Depends { name: "thetool" }
}
