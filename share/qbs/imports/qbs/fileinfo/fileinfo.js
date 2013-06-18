function path(fp) {
    if (fp[fp.length -1] === '/')
        return fp;
    var last = fp.lastIndexOf('/');
    if (last < 0)
        return '.';
    return fp.slice(0, last);
}

function fileName(fph) {
    var fp = fph.toString();
    var last = fp.lastIndexOf('/');
    if (last < 0)
        return fp;
    return fp.slice(last + 1);
}

function baseName(fph) {
    var fn = fileName(fph);
    return fn.split('.')[0];
}

function completeBaseName(fph) {
    var fn = fileName(fph);
    var last = fn.lastIndexOf(".");
    if (last < 0)
        return fn;
    else
        return fn.slice(0, last);
}

function relativePath(base, rel)
{
    var basel = base.split('/');
    var rell  = rel.split('/');
    var i;
    for (i = basel.length; i-- >= 0;) {
        if (basel[i] === '.' || basel[i] === '')
            basel.splice(i, 1);
    }
    for (i = rell.length; i-- >= 0;) {
        if (rell[i] === '.' || rell[i] === '')
            rell.splice(i, 1);
    }

    i = 0;
    while (i < basel.length && i < rell.length && basel[i] === rell[i])
        i++;

    var j = i;
    var r = [];

    for (; i < basel.length; i++)
        r.push('..');

    for (; j < rell.length; j++)
        r.push(rell[j]);

    return r.join('/');
}

var windowsAbsolutePathPattern = new RegExp("^[a-z,A-Z]:[/,\\\\]")
function isAbsolutePath(path)
{
    if (!path)
        return false;
    return (path.charAt(0) === '/' || windowsAbsolutePathPattern.test(path));
}

function toWindowsSeparators(str)
{
    return str.toString().replace(/\//g, '\\');
}

function fromWindowsSeparators(str)
{
    return str.toString().replace(/\\/g, '/');
}

var removeDoubleSlashesPattern = new RegExp("/{2,}", "g")

function joinPaths()
{
    function pathFilter(path) {
        return path && typeof path === 'string';
    }

    var paths = Array.prototype.slice.call(arguments, 0).filter(pathFilter);
    var joinedPaths = paths.join('/');

    return joinedPaths.replace(removeDoubleSlashesPattern, "/")
}
