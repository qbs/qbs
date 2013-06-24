//
// utility functions for modules
//

function moduleProperties(config, key)
{
    return config.moduleProperties(config.moduleName, key)
}

function modulePropertiesFromArtifacts(product, artifacts, moduleName, propertyName)
{
    var result = product.moduleProperties(moduleName, propertyName)
    for (var i in artifacts)
        result = result.concat(artifacts[i].moduleProperties(moduleName, propertyName))
    return result
}

function moduleProperty(product, propertyName)
{
    return product.moduleProperty(product.moduleName, propertyName)
}

function dumpProperty(key, value, level)
{
    var indent = ''
    for (var k=0; k < level; ++k)
        indent += '  '
    print(indent + key + ': ' + value)
}

function traverseObject(obj, func, level)
{
    if (!level)
        level = 0
    var i, children = {}
    for (i in obj) {
        if (typeof(obj[i]) === "object" && !(obj[i] instanceof Array))
            children[i] = obj[i]
        else
            func.apply(this, [i, obj[i], level])
    }
    level++
    for (i in children) {
        func.apply(this, [i, children[i], level - 1])
        traverseObject(children[i], func, level)
    }
    level--
}

function dumpObject(obj, description)
{
    if (!description)
        description = 'object dump'
    print('+++++++++ ' + description + ' +++++++++')
    traverseObject(obj, dumpProperty)
}


//////////////////////////////////////////////////////////
// The EnvironmentVariable class
//
function EnvironmentVariable(name, separator, convertPathSeparators)
{
    if (!name)
        throw "EnvironmentVariable c'tor needs a name as first argument."
    this.name = name
    this.value = getenv(name).toString()
    this.separator = separator || ''
    this.convertPathSeparators = convertPathSeparators || false
}

EnvironmentVariable.prototype.prepend = function(v)
{
    if (this.value.length > 0 && this.value.charAt(0) !== this.separator)
        this.value = this.separator + this.value
    if (this.convertPathSeparators)
        v = FileInfo.toWindowsSeparators(v)
    this.value = v + this.value
}

EnvironmentVariable.prototype.append = function(v)
{
    if (this.value.length > 0)
        this.value += this.separator
    if (this.convertPathSeparators)
        v = FileInfo.toWindowsSeparators(v)
    this.value += v
}

EnvironmentVariable.prototype.set = function()
{
    putenv(this.name, this.value)
}

