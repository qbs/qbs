import "dummy_base.qbs" as DummyBase

DummyBase {
    condition: true
    property stringList defines
    property stringList cFlags
    property stringList cxxFlags
}
