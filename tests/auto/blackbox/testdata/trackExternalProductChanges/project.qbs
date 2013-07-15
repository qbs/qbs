import qbs
import qbs.File
import "fileList.js" as FileList

CppApplication {
    property pathList filesFromEnv: qbs.getenv("QBS_TEST_PULL_IN_FILE_VIA_ENV") ? ["environmentChange.cpp"] : []
    property pathList filesFromJs: FileList.fileList()
    property pathList filesFromFs: File.exists(path + "/fileExists.cpp") ? ["fileExists.cpp"] : []
    files: ["main.cpp"].concat(filesFromJs).concat(filesFromEnv).concat(filesFromFs)
}
