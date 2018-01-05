import qbs

Project {
    CppApplication {
        name: "myapp"
        files: ["main.cpp"]
        Depends {
            name: "plugin1"                             // not to be linked
            cpp.link: qbs.hostOS === undefined
        }
        Depends { name: "plugin2" }                     // not to be linked
        Depends {
            name: "plugin3"                             // supposed to be linked
            //property bool theCondition: true
            cpp.link: /*theCondition && */product.name === "myapp"  // TODO: Make this work
        }
        Depends { name: "plugin4" }                     // supposed to be linked
        Depends { name: "helper" }                      // supposed to be linked
    }
    DynamicLibrary {
        name: "plugin1"
        files: ["plugin1.cpp"]
        Depends { name: "cpp" }
    }
    DynamicLibrary {
        name: "plugin2"
        files: ["plugin2.cpp"]
        Depends { name: "cpp" }
        Export {
            Parameters {
                cpp.link: false
            }
        }
    }
    DynamicLibrary {
        name: "plugin3"
        files: ["plugin3.cpp"]
        Depends { name: "cpp" }
        Export {
            Parameters {
                cpp.link: false
            }
        }
    }
    DynamicLibrary {
        name: "plugin4"
        files: ["plugin4.cpp"]
        Depends { name: "cpp" }
        Export {
            Parameters {
                // property bool theCondition: true
                cpp.link: true // theCondition TODO: Make this work
            }
        }
    }
    DynamicLibrary {
        name: "helper"
        files: ["helper.cpp"]
        Depends { name: "cpp" }
    }
}
