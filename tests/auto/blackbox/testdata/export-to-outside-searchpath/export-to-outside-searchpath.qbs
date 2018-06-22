Project {
    Project {
        qbsSearchPaths: ["qbs-resources"]
        Product {
            name: "otherProduct"
            Depends { name: "dep" }
        }
        Product {
            name: "dep"
            Export { Depends { name: "aModule" } }
        }
   }
   Project {
       Product {
           name: "theProduct"
           Depends { name: "dep" }
       }
   }
}
