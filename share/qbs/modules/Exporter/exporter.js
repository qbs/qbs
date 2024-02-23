/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

function getPrefixToStrip(project, product, propName, value)
{
    function checkValuePrefix(forbiddenPrefix, prefixDescription)
    {
        if (value.startsWith(forbiddenPrefix)) {
            throw "Value '" + value + "' for exported property '" + propName + "' in product '"
                    + product.name + "' points into " + prefixDescription + ".\n"
                    + "Did you forget to set the prefixMapping property in an Export item?";
        }
    }

    // Catch user oversights: Paths that point into the project source or build directories
    // make no sense in the module.
    if (!value.startsWith(product.qbs.installRoot)) {
        checkValuePrefix(project.buildDirectory, "project build directory");
        checkValuePrefix(project.sourceDirectory, "project source directory");
    }

    // Adapt file paths pointing into the install dir, that is, make them relative to the
    // module file for relocatability. We accept them with or without the install root.
    // The latter form will typically be a result of applying the prefixMapping property,
    // while the first one could be an untransformed path, for instance if the project
    // file is written in such a way that include paths are picked up from the installed
    // location rather than the source directory.
    var result;
    var fullInstallPrefix = FileInfo.joinPaths(product.qbs.installRoot, product.qbs.installPrefix);
    if (fullInstallPrefix.length > 1 && value.startsWith(fullInstallPrefix)) {
        result = fullInstallPrefix;
    } else {
        var installPrefix = FileInfo.joinPaths("/", product.qbs.installPrefix);
        if (installPrefix.length > 1 && value.startsWith(installPrefix))
            result = installPrefix;
    }
    return result;
}
