Project {
    references: ["test1", "test2", "test3"]
    AutotestRunner {
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("gcc")
        }
    }
}
