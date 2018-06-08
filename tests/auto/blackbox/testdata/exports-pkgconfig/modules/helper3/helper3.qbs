import qbs

Module {
    Depends { name: "cpp" }
    cpp.includePaths: "/somedir/include3"
}
