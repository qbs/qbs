import qbs
import qbs.Probes

CppApplication {
    Probes.PathProbe {
        id: probe1
        names: ["bin/tool"]
        platformPaths: [product.sourceDirectory]
    }

    Probes.PathProbe {
        id: probe2
        names: ["tool"]
        platformPaths: [product.sourceDirectory + "/bin"]
    }

    targetName: {
        console.info("probe1.fileName=" + probe1.fileName);
        console.info("probe1.path=" + probe1.path);
        console.info("probe1.filePath=" + probe1.filePath);

        console.info("probe2.fileName=" + probe2.fileName);
        console.info("probe2.path=" + probe2.path);
        console.info("probe2.filePath=" + probe2.filePath);

        return name;
    }

    files: ["main.c"]
}
