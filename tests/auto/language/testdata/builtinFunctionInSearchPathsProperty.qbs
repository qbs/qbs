import qbs.Environment

Project {
    qbsSearchPaths: {
        if (!Environment.getEnv("PATH"))
            throw "Environment.getEnv doesn't seem to work";
    }
}
