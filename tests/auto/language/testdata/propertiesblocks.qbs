import "propertiesblocks_base.qbs" as ProductBase

Project {
    Product {
        name: "property_overwrite"
        Depends { name: "dummy" }
        dummy.defines: ["SOMETHING"]
        Properties {
            condition: true
            dummy.defines: ["OVERWRITTEN"]
        }
    }
    Product {
        name: "property_set_indirect"
        Depends { name: "dummyqt.core" }
        Properties {
            condition: true
            dummyqt.core.zort: "VAL"
        }
    }
    Product {
        name: "property_overwrite_no_outer"
        Depends { name: "dummy" }
        Properties {
            condition: true
            dummy.defines: ["OVERWRITTEN"]
        }
    }
    Product {
        name: "property_append_to_outer"
        Depends { name: "dummy" }
        dummy.defines: ["ONE"]
        Properties {
            condition: true
            dummy.defines: outer.concat(["TWO"])
        }
    }
    Product {
        name: "property_append_to_indirect_outer"
        Depends { name: "dummy" }
        property stringList myDefines: ["ONE"]
        dummy.defines: myDefines
        Properties {
            condition: true
            dummy.defines: outer.concat(["TWO"])
        }
    }
    ProductBase {
        name: "property_append_to_indirect_derived_outer1"
        Properties {
            condition: true
            dummy.cFlags: outer.concat("PROPS")
        }
    }
    ProductBase {
        name: "property_append_to_indirect_derived_outer2"
        Properties {
            condition: true
            dummy.cFlags: outer.concat("PROPS")
        }
        dummy.cFlags: ["PRODUCT"]
    }
    ProductBase {
        name: "property_append_to_indirect_derived_outer3"
        Properties {
            condition: true
            dummy.cFlags: outer.concat("PROPS")
        }
        dummy.cFlags: base.concat("PRODUCT")
    }
    Product {
        name: "property_append_to_indirect_merged_outer"
        Depends { name: "dummy" }
        property string justOne: "ONE"
        dummy.rpaths: [justOne]
        Properties {
            condition: true
            dummy.rpaths: outer.concat(["TWO"])
        }
    }
    Product {
        name: "multiple_exclusive_properties"
        Depends { name: "dummy" }
        dummy.defines: ["SOMETHING"]
        Properties {
            condition: true
            dummy.defines: ["OVERWRITTEN"]
        }
        Properties {
            condition: false
            dummy.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "multiple_exclusive_properties_no_outer"
        Depends { name: "dummy" }
        Properties {
            condition: true
            dummy.defines: ["OVERWRITTEN"]
        }
        Properties {
            condition: false
            dummy.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "multiple_exclusive_properties_append_to_outer"
        Depends { name: "dummy" }
        dummy.defines: ["ONE"]
        Properties {
            condition: true
            dummy.defines: outer.concat(["TWO"])
        }
        Properties {
            condition: false
            dummy.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "ambiguous_properties"
        Depends { name: "dummy" }
        dummy.defines: ["ONE"]
        Properties {
            condition: true
            dummy.defines: outer.concat(["TWO"])
        }
        Properties {
            condition: false
            dummy.defines: outer.concat(["IMPOSSIBLE"])
        }
        Properties {    // will be ignored
            condition: true
            dummy.defines: outer.concat(["THREE"])
        }
    }
    Product {
        name: "condition_refers_to_product_property"
        property bool narf: true
        property string someString: "SOMETHING"
        Depends { name: "dummy" }
        Properties {
            condition: narf
            dummy.defines: ["OVERWRITTEN"]
            someString: "OVERWRITTEN"
        }
    }
    property bool zort: true
    Product {
        name: "condition_refers_to_project_property"
        property string someString: "SOMETHING"
        Depends { name: "dummy" }
        Properties {
            condition: project.zort
            dummy.defines: ["OVERWRITTEN"]
            someString: "OVERWRITTEN"
        }
    }
    ProductBase {
        name: "inheritance_overwrite_in_subitem"
        dummy.defines: ["OVERWRITTEN_IN_SUBITEM"]
    }
    ProductBase {
        name: "inheritance_retain_base1"
        dummy.defines: base.concat("SUB")
    }
    ProductBase {
        name: "inheritance_retain_base2"
        Properties {
            condition: true
            dummy.defines: base.concat("SUB")
        }
        dummy.defines: ["GNAMPF"]
    }
    ProductBase {
        name: "inheritance_retain_base3"
        Properties {
            condition: true
            dummy.defines: base.concat("SUB")
        }
        // no dummy.defines binding
    }
    ProductBase {
        name: "inheritance_retain_base4"
        Properties {
            condition: false
            dummy.defines: ["NEVERMORE"]
        }
        // no "else case" for dummy.defines. The value is derived from ProductBase.
    }
    ProductBase {
        name: "inheritance_condition_in_subitem1"
        defineBase: false
        dummy.defines: base.concat("SUB")
    }
    ProductBase {
        name: "inheritance_condition_in_subitem2"
        defineBase: false
        // no dummy.defines binding
    }
    Product {
        id: knolf
        name: "gnampf"
    }
    Product {
        name: "condition_references_id"
        Depends { name: "dummy" }
        Properties {
            condition: knolf.name === "gnampf"
            dummy.defines: ["OVERWRITTEN"]
        }
    }
    Product {
        name: "using_derived_Properties_item"
        Depends { name: "dummy" }
        MyProperties {
            condition: true
            dummy.defines: ["string from MyProperties"]
        }
    }
    Product {
        name: "conditional-depends"
        Depends {
            name: "dummy"
            condition: false
        }
        Properties {
            condition: false
            dummy.defines: ["a string"]
        }
    }
    Product {
        name: "use-module-with-properties-item"
        Depends { name: "module-with-properties-item" }
    }
}
