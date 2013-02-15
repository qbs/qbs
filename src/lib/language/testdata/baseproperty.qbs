import "baseproperty_base.qbs" as BaseProduct

BaseProduct {
    name: "product1"
    narf: base.concat(["boo"])
    zort: base.concat(["boo"])
}
