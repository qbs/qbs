import Collection as Collection1
import "collection" as Collection2

Product {
    name: "da product"
    targetName: Collection1.f1() + Collection1.f2() + Collection2.f1() + Collection2.f2()
}
