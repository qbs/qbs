import qbs.Probes

Project {

    CppApplication {
        Probes.PathProbe {
            id: probe1
            names: ["bin/tool"]
            platformSearchPaths: [product.sourceDirectory]
        }

        Probes.PathProbe {
            id: probe2
            names: ["tool"]
            platformSearchPaths: [product.sourceDirectory + "/bin"]
        }

        targetName: {
            console.info("probe1.fileName=" + probe1.fileName);
            console.info("probe1.path=" + probe1.path);
            console.info("probe1.filePath=" + probe1.filePath);

            console.info("probe2.fileName=" + probe2.fileName);
            console.info("probe2.path=" + probe2.path);
            console.info("probe2.filePath=" + probe2.filePath);

            console.info("probe3.fileName=" + probe3.fileName);
            console.info("probe3.path=" + probe3.path);
            console.info("probe3.filePath=" + probe3.filePath);
            return name;
        }
    }

    Probes.PathProbe {
        id: probe3
        names: ["tool"]
        platformSearchPaths: [project.sourceDirectory + "/bin"]
    }

}
