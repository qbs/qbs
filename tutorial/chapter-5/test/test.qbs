//! [0]
// test/test.qbs

MyAutoTest {
    Depends { name: "mylib" }
    name: "mytest"
    files: "test.c"
}
//! [0]
