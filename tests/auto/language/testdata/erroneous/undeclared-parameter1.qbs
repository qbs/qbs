Product {
    Depends { name: "prefix2.suffix" }
    Depends { name: "readonly"; prefix2.suffix.nope: "nope" }
}

