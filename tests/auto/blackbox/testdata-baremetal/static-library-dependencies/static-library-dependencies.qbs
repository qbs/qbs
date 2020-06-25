import "../BareMetalApplication.qbs" as BareMetalApplication
import "../BareMetalStaticLibrary.qbs" as BareMetalStaticLibrary

Project {
    BareMetalStaticLibrary {
        name: "lib-a"
        Depends { name: "cpp" }
        files: ["a1.c", "a2.c"]
    }
    BareMetalStaticLibrary {
        name: "lib-b"
        Depends { name: "cpp" }
        Depends { name: "lib-a" }
        files: ["b.c"]
    }
    BareMetalStaticLibrary {
        name: "lib-c"
        Depends { name: "cpp" }
        Depends { name: "lib-a" }
        files: ["c.c"]
    }
    BareMetalStaticLibrary {
        name: "lib-d"
        Depends { name: "cpp" }
        Depends { name: "lib-b" }
        Depends { name: "lib-c" }
        files: ["d.c"]
    }
    BareMetalStaticLibrary {
        name: "lib-e"
        Depends { name: "cpp" }
        Depends { name: "lib-d" }
        files: ["e.c"]
    }
    BareMetalApplication {
        name: "app"
        Depends { name: "lib-e" }
        files: ["app.c"]
    }
}
