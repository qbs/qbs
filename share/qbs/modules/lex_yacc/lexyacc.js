var FileInfo = require("qbs.FileInfo");

function outputFilePath(product, input, posixFileName, forYacc)
{
    var outDir = input.lex_yacc.outputDir;
    var fileName;
    if (input.lex_yacc.uniqueSymbolPrefix) {
        fileName = input.baseName;
        if (forYacc)
            fileName += posixFileName.slice(1);
        else
            fileName += posixFileName;
    } else {
        fileName = posixFileName;
    }
    return FileInfo.joinPaths(outDir, fileName);
}
