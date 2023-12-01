Module {
    property string directory
    property string fileName
    Group {
        prefix: directory + "/"
        files: fileName
    }
}
