function rfc1034(inStr)
{
    return inStr.replace(/[^-A-Za-z0-9.]/g,'-');
}

// Returns the localization of a resource at the given path,
// or undefined if the path does not contain an {xx}.lproj path segment
function localizationKey(path)
{
    return _resourceFileProperties(path)[0];
}

// Returns the path of a localized resource at the given path,
// relative to its containing {xx}.lproj directory, or '.'
// if the resource is unlocalized (not contained in an lproj directory)
function relativeResourcePath(path)
{
    return _resourceFileProperties(path)[1];
}

function _resourceFileProperties(path)
{
    var lprojKey = ".lproj/";
    var lproj = path.indexOf(lprojKey);
    if (lproj >= 0) {
        // The slash preceding XX.lproj
        var slashIndex = path.slice(0, lproj).lastIndexOf('/');
        if (slashIndex >= 0) {
            var localizationKey = path.slice(slashIndex + 1, lproj);
            var relativeResourcePath = FileInfo.path(path.slice(lproj + lprojKey.length));
            return [ localizationKey, relativeResourcePath ];
        }
    }

    return [ undefined, '.' ];
}
