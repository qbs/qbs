import "conditionaldepends_base.qbs" as CondBase

Project {
    CondBase {
        name: 'conditionaldepends_derived'
        someProp: true
    }

    CondBase {
        name: 'conditionaldepends_derived_false'
        someProp: "knolf" === "narf"
    }

    Product {
        name: "product_props_true"
        property bool someTrueProp: true
        Depends { condition: someTrueProp; name: "dummy"}
    }

    Product {
        name: "product_props_false"
        property bool someFalseProp: false
        Depends { condition: someFalseProp; name: "dummy"}
    }

    property bool someTruePrjProp: true
    Product {
        name: "project_props_true"
        Depends { condition: project.someTruePrjProp; name: "dummy"}
    }

    property bool someFalsePrjProp: false
    Product {
        name: "project_props_false"
        Depends { condition: project.someFalsePrjProp; name: "dummy"}
    }

    Product {
        name: "module_props_true"
        Depends { name: "dummy2" }
        Depends { condition: dummy2.someTrueProp; name: "dummy" }
    }

    Product {
        name: "module_props_false"
        Depends { name: "dummy2" }
        Depends { condition: dummy2.someFalseProp; name: "dummy" }
    }

    Product {
        name: "multilevel_module_props_true"
        Depends { name: "dummy3" }
        dummy3.loadDummy: true
    }

    Product {
        name: "multilevel_module_props_false"
        Depends { name: "dummy3" }
    }

    Product {
        name: "multilevel_module_props_overridden"
        Depends { name: "dummy3" }
    }

    Product {
        name: "multilevel2_module_props_true"
        Depends { name: "dummy3_loader" }
    }

    Product {
        name: "contradictory_conditions1"
        Depends { condition: false; name: "dummy" }
        Depends { condition: true; name: "dummy" }  // this one wins
    }

    Product {
        name: "contradictory_conditions2"
        Depends { condition: true; name: "dummy" }  // this one wins
        Depends { condition: false; name: "dummy" }
    }

    Product {
        name: "unknown_dependency_condition_false"
        Depends { condition: false; name: "doesonlyexistifhellfreezesover" }
    }
}
