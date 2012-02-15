//
// utility functions for modules
//

function appendAll_internal2(modules, name, property, seenValues)
{
    var result = []
    for (var m in modules) {
        if (m == name) {
            var value = modules[m][property]
            if (value) {
                if (!seenValues[value]) {
                    seenValues[value] = true
                    result = result.concat(value)
                }
            }
        } else {
            var values = appendAll_internal2(modules[m].modules, name, property, seenValues)
            if (values && values.length > 0)
                result = result.concat(values)
        }
    }
    return result
}

function appendAll_internal(modules, name, property)
{
    var seenValues = []
    return appendAll_internal2(modules, name, property, seenValues)
}

function appendAll(config, property)
{
    return appendAll_internal(config.modules, config.module.name, property)
}

function appendAllFromArtifacts(product, artifacts, moduleName, propertyName)
{
    var seenValues = []
    var result = appendAll_internal2(product.modules, moduleName, propertyName, seenValues)
    for (var i in artifacts)
        result = result.concat(appendAll_internal2(artifacts[i].modules, moduleName, propertyName, seenValues))
    return result
}

function findFirst(modules, name, property)
{
    for (var m in modules) {
        if (m == name) {
            var value = modules[m][property]
            if (value)
                return value
        } else {
            var value = findFirst(m.modules, name, property)
            if (value)
                return value
        }
    }
    return null
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
    var children = {}
    for (var i in obj) {
        if (typeof(obj[i]) == "object" && !(obj[i] instanceof Array))
            children[i] = obj[i]
        else
            func.apply(this, [i, obj[i], level])
    }
    level++
    for (var i in children) {
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
    if (this.value.length > 0 && this.value.charAt(0) != this.separator)
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

