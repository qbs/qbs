/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
//
// utility functions for modules
//

/*!
 * Given a list of file tags, returns the file tag (one of [c, cpp, objc, objcpp])
 * corresponding to the C-family language the file should be compiled as.
 *
 * If no such tag is found, undefined is returned. If more than one match is
 * found, an exception is thrown.
 */
function fileTagForTargetLanguage(fileTags)
{
    var srcTags = ["c", "cpp", "objc", "objcpp", "asm", "asm_cpp"];
    var pchTags = ["c_pch", "cpp_pch", "objc_pch", "objcpp_pch"];

    var canonicalTag = undefined;
    var foundTagCount = 0;
    for (var i = 0; i < fileTags.length; ++i) {
        var idx = srcTags.indexOf(fileTags[i]);
        if (idx === -1)
            idx = pchTags.indexOf(fileTags[i]);

        if (idx !== -1) {
            canonicalTag = srcTags[idx];
            if (++foundTagCount > 1)
                break;
        }
    }

    if (foundTagCount > 1)
        throw ("source files cannot be identified as more than one language");

    return foundTagCount == 1 ? canonicalTag : undefined;
}

/*
 * Returns the name of a language-specific property given the file tag
 * for that property, and the base property name.
 *
 * If \a fileTag is undefined, the language-agnostic property name is returned.
 *
 * \param propertyName flags, platformFlags, precompiledHeader
 * \param fileTag c, cpp, objc, objcpp
 */
function languagePropertyName(propertyName, fileTag)
{
    if (!fileTag)
        fileTag = 'common';

    var map = {
        'c': {
            'flags': 'cFlags',
            'platformFlags': 'platformCFlags',
            'precompiledHeader': 'cPrecompiledHeader'
        },
        'cpp': {
            'flags': 'cxxFlags',
            'platformFlags': 'platformCxxFlags',
            'precompiledHeader': 'cxxPrecompiledHeader'
        },
        'objc': {
            'flags': 'objcFlags',
            'platformFlags': 'platformObjcFlags',
            'precompiledHeader': 'objcPrecompiledHeader'
        },
        'objcpp': {
            'flags': 'objcxxFlags',
            'platformFlags': 'platformObjcxxFlags',
            'precompiledHeader': 'objcxxPrecompiledHeader'
        },
        'common': {
            'flags': 'commonCompilerFlags',
            'platformFlags': 'platformCommonCompilerFlags',
            'precompiledHeader': 'precompiledHeader'
        }
    };

    var lang = map[fileTag];
    if (!lang)
        return propertyName;

    return lang[propertyName] || propertyName;
}

function moduleProperties(config, key, langFilter)
{
    return config.moduleProperties(config.moduleName, languagePropertyName(key, langFilter))
}

function modulePropertiesFromArtifacts(product, artifacts, moduleName, propertyName, langFilter)
{
    var result = product.moduleProperties(moduleName, languagePropertyName(propertyName, langFilter))
    for (var i in artifacts)
        result = result.concat(artifacts[i].moduleProperties(moduleName, languagePropertyName(propertyName, langFilter)))
    return result
}

function moduleProperty(product, propertyName, langFilter)
{
    return product.moduleProperty(product.moduleName, languagePropertyName(propertyName, langFilter))
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

function uniqueConcat(array1, array2)
{
    var result = array1;
    for (i in array2) {
        var elem = array2[i];
        if (result.indexOf(elem) === -1)
            result.push(elem);
    }
    return result;
}


//////////////////////////////////////////////////////////
// The EnvironmentVariable class
//
function EnvironmentVariable(name, separator, convertPathSeparators)
{
    if (!name)
        throw "EnvironmentVariable c'tor needs a name as first argument."
    this.name = name
    this.value = getEnv(name).toString()
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
    putEnv(this.name, this.value)
}

