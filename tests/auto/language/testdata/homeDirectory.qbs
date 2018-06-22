Project {
    Product {
        name: "home"

        // These should resolve
        property path home: "~"
        property path homeSlash: "~/"
        property path homeUp: "~/.."
        property path homeFile: "~/a"

        // These are sanity checks and should not
        property path bogus1: "a~b"
        property path bogus2: "a/~/bb"
        property path user: "~foo/bar" // we don't resolve other-user paths
    }
}
