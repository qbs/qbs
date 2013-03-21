import qbs 1.0
import "propertiesblocks_base.qbs" as ProductBase

Project {
    Product {
        name: "property_overwrite"
        Depends { id: cpp; name: "dummy" }
        cpp.defines: ["SOMETHING"]
        Properties {
            condition: true
            cpp.defines: ["OVERWRITTEN"]
        }
    }
    Product {
        name: "property_overwrite_no_outer"
        Depends { id: cpp; name: "dummy" }
        Properties {
            condition: true
            cpp.defines: ["OVERWRITTEN"]
        }
    }
    Product {
        name: "property_append_to_outer"
        Depends { id: cpp; name: "dummy" }
        cpp.defines: ["ONE"]
        Properties {
            condition: true
            cpp.defines: outer.concat(["TWO"])
        }
    }
    Product {
        name: "multiple_exclusive_properties"
        Depends { id: cpp; name: "dummy" }
        cpp.defines: ["SOMETHING"]
        Properties {
            condition: true
            cpp.defines: ["OVERWRITTEN"]
        }
        Properties {
            condition: false
            cpp.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "multiple_exclusive_properties_no_outer"
        Depends { id: cpp; name: "dummy" }
        Properties {
            condition: true
            cpp.defines: ["OVERWRITTEN"]
        }
        Properties {
            condition: false
            cpp.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "multiple_exclusive_properties_append_to_outer"
        Depends { id: cpp; name: "dummy" }
        cpp.defines: ["ONE"]
        Properties {
            condition: true
            cpp.defines: outer.concat(["TWO"])
        }
        Properties {
            condition: false
            cpp.defines: ["IMPOSSIBLE"]
        }
    }
    Product {
        name: "ambiguous_properties"
        Depends { id: cpp; name: "dummy" }
        cpp.defines: ["ONE"]
        Properties {
            condition: true
            cpp.defines: outer.concat(["TWO"])
        }
        Properties {
            condition: false
            cpp.defines: outer.concat(["IMPOSSIBLE"])
        }
        Properties {    // will be ignored
            condition: true
            cpp.defines: outer.concat(["THREE"])
        }
    }
    Product {
        name: "condition_refers_to_product_property"
        property string narf: true
        Depends { name: "dummy" }
        Properties {
            condition: narf
            dummy.defines: ["OVERWRITTEN"]
        }
    }
    property string zort: true
    Product {
        name: "condition_refers_to_project_property"
        Depends { name: "dummy" }
        Properties {
            condition: project.zort
            dummy.defines: ["OVERWRITTEN"]
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
        Depends { id: cpp; name: "dummy" }
        Properties {
            condition: knolf.name === "gnampf"
            cpp.defines: ["OVERWRITTEN"]
        }
    }
}
