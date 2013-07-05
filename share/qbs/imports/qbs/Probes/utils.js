//
// utility functions for probes
//

function concatAll()
{
    var result = [];
    for (var i = 0; i < arguments.length; ++i) {
        var arg = arguments[i];
        if (arg === undefined)
            continue;
        else if (arg instanceof Array)
            result = result.concat(arg);
        else
            result.push(arg);
    }
    return result;
}
