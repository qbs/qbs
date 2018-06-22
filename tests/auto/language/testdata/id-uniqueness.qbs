import "idusagebase.qbs" as DerivedProduct

Project {
    id: theProject
    DerivedProduct {
        id: baseProduct         // OK - even though 'baseProduct' is used in the base item.
    }
    DerivedProduct {
        id: baseProduct         // ERROR
    }
}
