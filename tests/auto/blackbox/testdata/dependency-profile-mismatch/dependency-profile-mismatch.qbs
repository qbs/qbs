import qbs

Project {
    property string mainProfile
    property string depProfile
    Product {
        name: "dep"
        profiles: [project.depProfile]
    }
    Product {
        name: "main"
        Depends { name: "dep"; profiles: [project.mainProfile]; }
    }
}
