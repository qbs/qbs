import qbs
import qbs.File
import "fileList.js" as FileList

CppApplication {
    files: ["main.cpp"].concat(FileList.fileList()).concat(FileList.filesFromEnv(qbs)).concat(FileList.filesFromFs(qbs))
}
