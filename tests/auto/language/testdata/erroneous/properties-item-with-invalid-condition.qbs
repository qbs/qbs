Product {
    Depends  { name: "cpp" }
    Properties {
       condition: cpp.nonexistingproperty.contains("somevalue")
       cpp.defines: ["ABC"]
    }
}
