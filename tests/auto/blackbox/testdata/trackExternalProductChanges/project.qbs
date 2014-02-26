import qbs
import qbs.File
import "fileList.js" as FileList

CppApplication {
    property stringList filesFromEnv: qbs.getEnv("QBS_TEST_PULL_IN_FILE_VIA_ENV")
                                      ? ["environmentChange.cpp"] : []
    files: ["main.cpp"].concat(FileList.fileList()).concat(filesFromEnv).concat(FileList.filesFromFs(qbs))
}
