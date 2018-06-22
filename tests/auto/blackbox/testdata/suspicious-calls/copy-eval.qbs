import qbs.File

Product {
    name: {
        File.copy(sourceDirectory + "/copy-eval.qbs",
                  sourceDirectory + "/copy-eval2.qbs");
        return "blubb"
    }
}
