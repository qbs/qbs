import qbs

Group {
    prefix: product.sourceDirectory + '/'
    files: ["referenced-via-absolute-prefix.txt"]
}
