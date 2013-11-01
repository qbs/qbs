var TextFile = loadExtension("qbs.TextFile");

// TODO: Parse exactly, so we won't get fooled by e.g. comments.
function extractPackageName(filePath)
{
    var file = new TextFile(filePath);
    var contents = file.readAll();
    file.close();
    var packageName = contents.replace(/[\s\S]*package\s+([^\s;]+)[\s;][\s\S]*/m, "$1");
    if (packageName === contents) // no package statement
        return "";
    return packageName;
}
