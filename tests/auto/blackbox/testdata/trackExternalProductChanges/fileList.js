var File = require("qbs.File");

function fileList() { return []; }

function filesFromFs(path) { return File.exists(path + "/fileExists.cpp") ? ["fileExists.cpp"] : []; }

