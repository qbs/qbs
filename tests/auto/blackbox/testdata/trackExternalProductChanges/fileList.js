function fileList() { return []; }

function filesFromEnv(qbs) { return qbs.getenv("QBS_TEST_PULL_IN_FILE_VIA_ENV") ? ["environmentChange.cpp"] : []; }

function filesFromFs(qbs) { return File.exists(path + "/fileExists.cpp") ? ["fileExists.cpp"] : []; }

