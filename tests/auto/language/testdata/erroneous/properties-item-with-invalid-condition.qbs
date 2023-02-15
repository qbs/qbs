Product {
    Depends  { name: "cpp" }
    Properties {
       condition: cpp.nonexistingproperty.includes("somevalue")
       cpp.defines: ["ABC"]
    }
}
