var FileInfo = require("qbs.FileInfo");
var Utilities = require("qbs.Utilities");
var Xml = require("qbs.Xml");

function fullPath(product)
{
    if (Utilities.versionCompare(product.Qt.core.version, "6.1") < 0)
        return product.Qt.core.binPath + '/' + product.Qt.core.rccName;
    return product.Qt.core.libExecPath + '/' + product.Qt.core.rccName;
}

function generateQrcFileCommands(project, product, inputs, outputs, input, output,
                                 explicitlyDependsOn)
{
    var cmd = new JavaScriptCommand();
    cmd.description = "generating " + output.fileName;
    cmd.sourceCode = function() {
        var doc = new Xml.DomDocument("RCC");

        var rccNode = doc.createElement("RCC");
        rccNode.setAttribute("version", "1.0");
        doc.appendChild(rccNode);

        var inputsByPrefix = {}
        for (var i = 0; i < inputs["qt.core.resource_data"].length; ++i) {
            var inp = inputs["qt.core.resource_data"][i];
            var prefix = inp.Qt.core.resourcePrefix;
            var inputsList = inputsByPrefix[prefix] || [];
            inputsList.push(inp);
            inputsByPrefix[prefix] = inputsList;
        }

        for (var prefix in inputsByPrefix) {
            var qresourceNode = doc.createElement("qresource");
            qresourceNode.setAttribute("prefix", prefix);
            rccNode.appendChild(qresourceNode);

            for (var i = 0; i < inputsByPrefix[prefix].length; ++i) {
                var inp = inputsByPrefix[prefix][i];
                var fullResPath = inp.filePath;
                var baseDir = inp.Qt.core.resourceSourceBase;
                var resAlias = baseDir
                    ? FileInfo.relativePath(baseDir, fullResPath) : inp.fileName;

                var fileNode = doc.createElement("file");
                fileNode.setAttribute("alias", resAlias);
                qresourceNode.appendChild(fileNode);

                var fileTextNode = doc.createTextNode(fullResPath);
                fileNode.appendChild(fileTextNode);
            }
        }

        doc.save(output.filePath, 4);
    };
    return [cmd];
}

function rccOutputArtifacts(input)
{
    var artifact = {
        filePath: "qrc_" + input.completeBaseName + ".cpp",
        fileTags: ["cpp"]
    };
    if (input.Qt.core.enableBigResources)
        artifact.fileTags.push("cpp_intermediate_object");
    return [artifact];
}

function rccCommands(product, input, output)
{
    var args = [input.filePath,
                "-name", FileInfo.completeBaseName(input.filePath),
                "-o", output.filePath];
    if (input.Qt.core.enableBigResources)
        args.push("-pass", "1");
    if (product.Qt.core.disabledFeatures.contains("zstd")
            && Utilities.versionCompare(product.Qt.core.version, "5.13") >= 0) {
        args.push("--no-zstd");
    }
    var cmd = new Command(fullPath(product), args);
    cmd.description = "rcc "
        + (input.Qt.core.enableBigResources ? "(pass 1) " : "")
        + input.fileName;
    cmd.highlight = 'codegen';
    return cmd;
}

function rccPass2Commands(product, input, output)
{
    function findChild(artifact, predicate) {
        var children = artifact.children;
        var len = children.length;
        for (var i = 0; i < len; ++i) {
            var child = children[i];
            if (predicate(child))
                return child;
            child = findChild(child, predicate);
            if (child)
                return child;
        }
        return undefined;
    }
    var qrcArtifact = findChild(input, function(c) { return c.fileTags.contains("qrc"); });
    var cppArtifact = findChild(input, function(c) { return c.fileTags.contains("cpp"); });
    var cmd = new Command(fullPath(product),
                          [qrcArtifact.filePath,
                           "-temp", input.filePath,
                           "-name", FileInfo.completeBaseName(input.filePath),
                           "-o", output.filePath,
                           "-pass", "2"]);
    cmd.description = "rcc (pass 2) " + qrcArtifact.fileName;
    cmd.highlight = 'codegen';
    return cmd;
}
