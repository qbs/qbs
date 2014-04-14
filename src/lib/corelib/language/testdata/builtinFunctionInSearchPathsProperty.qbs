import qbs

Project {
    qbsSearchPaths: {
        if (!qbs.getEnv("PATH"))
            throw "qbs.getEnv doesn't seem to work";
    }
}
