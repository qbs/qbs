// replace chars non safe for a domain name (rfc1034) with a "-"
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

// perform replacements in env recursively
// JSON.stringify(doRepl({a:"$(x)3$$(y)",b:{t:"%$(y) $(k)"}},{x:"X",y:"Y"}, true))
//    Warning undefined variable  k  in variable expansion
// => {"a":"X3$Y","b":{"t":"%Y $(k)"}}
function doRepl(obj, env, warn)
{
    function doReplR(obj, env, checked) {
        checked.push(obj);
        for (var key in obj) {
            var value =obj[key];
            var type = typeof(value);
            if (type === "object") {
                if (checked.indexOf(value) !== -1)
                    continue;
                doReplR(value, env, checked);
            }
            if (type !== "string")
                continue;
            var i = value.indexOf('$')
            var changes = false;
            while (i !== -1) {
                if (value.charAt(i+1) === '(') {
                    var j = value.indexOf(')', i + 2);
                    if (j === -1)
                        break;
                    var varName = value.slice(i + 2, j);
                    var varValue = env[varName];
                    if (undefined === varValue) {
                        // skip replacement
                        if (warn)
                            print("Warning undefined variable ",varName, " in variable expansion");
                    } else {
                        changes = true;
                        varValue = String(varValue);
                        value = value.slice(0, i) + varValue + value.slice(j + 1);
                        // avoid recursive substitutions to avoid potentially infinite loops
                        i += varValue.length;
                    }
                }
                i = value.indexOf('$', i + 1);
            }
            if (changes)
                obj[key] = value;
        }
    }
    doReplR(obj, env, []);
    return obj;
}
