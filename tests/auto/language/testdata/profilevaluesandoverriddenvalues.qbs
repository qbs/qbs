Project {
    Application {
        name: "product1"
        property bool dummyProp: {
            if (!(dummy.cFlags instanceof Array))
                throw new Error("dummy.cFlags: Array type expected.");
            if (!(dummy.cxxFlags instanceof Array))
                throw new Error("dummy.cxxFlags: Array type expected.");
            if (!(dummy.defines instanceof Array))
                throw new Error("dummy.defines: Array type expected.");
            return true;
        }
        consoleApplication: true
        Depends { name: "dummy" }
        // dummy.cxxFlags is set via profile and is not overridden
        dummy.defines: ["IN_FILE"]  // set in profile, overridden in file
        dummy.cFlags: ["IN_FILE"]   // set in profile, overridden on command line
    }
}
