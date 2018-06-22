import qbs.File

Project {
    condition: File.exists("blubb");
    references: "blubb/nosuchfile.qbs"
}
