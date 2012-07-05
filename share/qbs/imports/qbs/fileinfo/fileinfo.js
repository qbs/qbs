function path(fp) {
    if (fp[fp.length -1] == '/')
        return fp;
    var last = fp.lastIndexOf('/');
    if (last < 0)
        return '.';
    return fp.slice(0, last);
}

function fileName(fph) {
    var fp = fph.toString();
    if (fp[fp.length -1] == '/')
        return fp;
    var last = fp.lastIndexOf('/');
    if (last < 0)
        return '.';
    return fp.slice(last + 1);
}

function baseName(fph) {
    var fn = fileName(fph);
    return fn.split('.')[0];
}

function relativePath(base, rel)
{
    var basel = base.split('/');
    var rell  = rel.split('/');
    var i = 0;

    while (i < basel.length && i < rell.length && basel[i] == rell[i])
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
    return (path.charAt(0) == '/' || windowsAbsolutePathPattern.test(path));
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
