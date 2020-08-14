Module {
    Depends { name: "dummy"; condition: { console.info("product: " + product.name); return false; } }
}
