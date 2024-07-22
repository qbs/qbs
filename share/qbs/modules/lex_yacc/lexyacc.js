var FileInfo = require("qbs.FileInfo");
var TextFile = require("qbs.TextFile");

function unquote(s)
{
    return s.startsWith('"') && s.endsWith('"') ? s.substr(1, s.length - 2) : s;
}

function readLexOptions(filePath) {
    var result = {};
    var f = new TextFile(filePath, TextFile.ReadOnly);
    var regex = /^%option\s+([^ \t=]+)(?:\s*=\s*(\S+))?/;
    while (!f.atEof()) {
        var line = f.readLine();
        var m = regex.exec(line);
        if (!m) {
            if (line === "%%")
                break;
            continue;
        }
        result[m[1]] = m[2] || true;
    }
    f.close();
    return result;
}

function lexOutputFilePath(input, posixFileName, options)
{
    var outDir = input.lex_yacc.outputDir;
    var fileName;
    if (options.outfile) {
        fileName = unquote(options.outfile);
    } else if (options.prefix) {
        fileName = FileInfo.baseName(posixFileName) + '.'
                + unquote(options.prefix) + '.'
                + FileInfo.suffix(posixFileName);
    } else if (input.lex_yacc.uniqueSymbolPrefix) {
        fileName = input.baseName;
        fileName += posixFileName;
    } else {
        fileName = posixFileName;
    }
    return FileInfo.joinPaths(outDir, fileName);
}

function readYaccOptions(filePath) {
    var result = {};
    var f = new TextFile(filePath, TextFile.ReadOnly);
    var regex = /^%output\s+(.+)/;
    while (!f.atEof()) {
        var line = f.readLine();
        var m = regex.exec(line);
        if (!m) {
            if (line === "%%")
                break;
            continue;
        }
        result.output = m[1];
        break;
    }
    f.close();
    return result;
}

function yaccOutputFilePath(input, posixFileName, options)
{
    var outDir = input.lex_yacc.outputDir;
    var fileName;
    if (options.output) {
        var outputFileName = unquote(options.output);
        var suffix = FileInfo.suffix(posixFileName);
        if (suffix === "c") {
            fileName = outputFileName;
        } else {
            fileName = FileInfo.completeBaseName(outputFileName)
                    + '.' + suffix + FileInfo.suffix(outputFileName).slice(1);
        }
    } else if (input.lex_yacc.uniqueSymbolPrefix) {
        fileName = input.baseName + posixFileName.slice(1);
    } else {
        fileName = posixFileName;
    }
    return FileInfo.joinPaths(outDir, fileName);
}

function yaccOutputArtifacts(product, input)
{
    var src = {
        fileTags: [product.lex_yacc.outputTag],
        lex_yacc: {},
    };
    var options = readYaccOptions(input.filePath);
    if (!options.output && input.lex_yacc.yaccOutputFilePath) {
        options.output = input.lex_yacc.yaccOutputFilePath;
        src.lex_yacc.useOutputFromModule = true;
    }
    var hdr = {
        filePath: yaccOutputFilePath(input, "y.tab.h", options),
        fileTags: ["hpp"],
    };
    src.filePath = yaccOutputFilePath(input, "y.tab.c", options);
    src.cpp = {
        includePaths: [].concat(input.cpp.includePaths, input.lex_yacc.outputDir),
        warningLevel: input.lex_yacc.enableCompilerWarnings ? "all" : "none",
    };
    return [hdr, src];
}

function yaccCommands(project, product, inputs, outputs, input, output, explicitlyDependsOn)
{
    var args = input.lex_yacc.yaccFlags;
    args.push("-d");
    var impl = outputs[input.lex_yacc.outputTag][0];
    if (impl.lex_yacc.useOutputFromModule)
        args.push("-o" + input.lex_yacc.yaccOutputFilePath);
    else if (input.lex_yacc.uniqueSymbolPrefix)
        args.push("-b", input.baseName, "-p", input.baseName);
    args.push(input.filePath);
    var cmd = new Command(input.lex_yacc.yaccBinary, args);
    cmd.workingDirectory = input.lex_yacc.outputDir;
    cmd.description = "generating "
            + impl.fileName
            + " and " + outputs["hpp"][0].fileName;
    return [cmd];
}

function lexOutputArtifacts(product, input)
{
    var output = {
        fileTags: [product.lex_yacc.outputTag],
        lex_yacc: {},
    };
    var options = readLexOptions(input.filePath);
    if (!options.outfile && input.lex_yacc.lexOutputFilePath) {
        options.outfile = input.lex_yacc.lexOutputFilePath;
        output.lex_yacc.useOutfileFromModule = true;
    }
    output.filePath = lexOutputFilePath(input, "lex.yy.c", options);
    output.cpp = {
        includePaths: [].concat(input.cpp.includePaths, input.lex_yacc.outputDir),
        warningLevel: input.lex_yacc.enableCompilerWarnings ? "all" : "none",
    };
    return [output];
}

function lexCommands(project, product, inputs, outputs, input, output, explicitlyDependsOn)
{
    var args = input.lex_yacc.lexFlags;
    if (output.lex_yacc.useOutfileFromModule)
        args.push("-o" + input.lex_yacc.lexOutputFilePath);
    else if (input.lex_yacc.uniqueSymbolPrefix)
        args.push("-P" + input.baseName, "-o" + output.filePath);
    args.push(input.filePath);
    var cmd = new Command(input.lex_yacc.lexBinary, args);
    cmd.workingDirectory = input.lex_yacc.outputDir;
    cmd.description = "generating " + output.fileName;
    return [cmd];
}
