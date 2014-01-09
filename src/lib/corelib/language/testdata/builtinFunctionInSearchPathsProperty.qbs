import qbs

Project {
    qbsSearchPaths: {
        if (!qbs.getenv("PATH"))
            throw "getenv doesn't seem to work";
    }
}
