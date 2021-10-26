/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PC_PARSER_H
#define PC_PARSER_H

#include "pcpackage.h"

namespace qbs {

class PkgConfig;

class PcParser
{
public:
    explicit PcParser(const PkgConfig &pkgConfig);

    PcPackageVariant parsePackageFile(const std::string &path);

private:
    std::string trimAndSubstitute(const PcPackage &pkg, std::string_view str) const;
    void parseStringField(
            PcPackage &pkg,
            std::string &field,
            std::string_view fieldName,
            std::string_view str);
    void parseLibs(
            PcPackage &pkg,
            std::vector<PcPackage::Flag> &libs,
            std::string_view fieldName,
            std::string_view str);
    std::vector<PcPackage::Flag> doParseLibs(const std::vector<std::string> &argv);
    void parseCFlags(PcPackage &pkg, std::string_view str);
    std::vector<PcPackage::RequiredVersion> parseModuleList(PcPackage &pkg, std::string_view str);
    void parseVersionsField(
            PcPackage &pkg,
            std::vector<PcPackage::RequiredVersion> &modules,
            std::string_view fieldName,
            std::string_view str);
    void parseLine(PcPackage &pkg, std::string_view str);

private:
    const PkgConfig &m_pkgConfig;
};

} // namespace qbs

#endif // PC_PARSER_H
