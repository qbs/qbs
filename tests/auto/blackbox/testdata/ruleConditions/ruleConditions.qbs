import "templates/zorduct.qbs" as Zorduct

Project {
    Zorduct {
        narfzort.buildZort: false
        name: "unzorted"
    }
    Zorduct {
        name: "zorted"
    }
}
