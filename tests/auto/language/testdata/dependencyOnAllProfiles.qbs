Project {
    property string profile1
    property string profile2

    Product {
        name: "dep"
        qbs.profiles: [project.profile1, project.profile2]
    }

    Product {
        name: "main"
        Depends {
            name: "dep"
        }
    }
}
