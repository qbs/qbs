import qbs
import "fileList.js" as FileList

CppApplication {
    files: ["main.cpp"].concat(FileList.fileList()).concat(qbs.getenv("QBS_TEST_PULL_IN_FILE_VIA_ENV") ? ["environmentChange.cpp"] : [])
}
