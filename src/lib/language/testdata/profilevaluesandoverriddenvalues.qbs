import qbs 1.0

Project {
    Application {
        name: "product1"
        Depends { name: "dummy" }
        // dummy.cxxFlags is set via profile and is not overridden
        dummy.defines: ["IN_FILE"]  // set in profile, overridden in file
        dummy.cFlags: ["IN_FILE"]   // set in profile, overridden on command line
    }
}
