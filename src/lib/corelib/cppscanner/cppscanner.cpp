/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com).
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

#include "cppscanner.h"
#include "../../plugins/scanner/scanner.h"
#include "Lexer.h"

#include <cstring>
#include <memory>

namespace qbs::Internal {

using namespace CPlusPlus;

class TokenComparator
{
    const char * const m_fileContent;

public:
    TokenComparator(const char *fileContent)
        : m_fileContent(fileContent)
    {}

    bool equals(const Token &tk, const QLatin1String &literal) const
    {
        return static_cast<int>(tk.length()) == literal.size()
               && memcmp(m_fileContent + tk.begin(), literal.data(), literal.size()) == 0;
    }
};

static void doScanCppFile(
    CppScannerContext &context,
    CPlusPlus::Lexer &yylex,
    bool scanForFileTags,
    bool scanForDependencies)
{
    const QLatin1String includeLiteral("include");
    const QLatin1String importLiteral("import");
    const QLatin1String defineLiteral("define");
    const QLatin1String qobjectLiteral("Q_OBJECT");
    const QLatin1String qgadgetLiteral("Q_GADGET");
    const QLatin1String qnamespaceLiteral("Q_NAMESPACE");
    const QLatin1String pluginMetaDataLiteral("Q_PLUGIN_METADATA");

    const TokenComparator tc(context.fileContent);
    Token tk;
    Token oldTk;
    ScanResult scanResult;

    yylex(&tk);

    while (tk.isNot(T_EOF_SYMBOL)) {
        if (tk.newline() && tk.is(T_POUND)) {
            yylex(&tk);

            if (scanForDependencies && !tk.newline() && tk.is(T_IDENTIFIER)) {
                if (tc.equals(tk, includeLiteral) || tc.equals(tk, importLiteral)) {
                    yylex.setScanAngleStringLiteralTokens(true);
                    yylex(&tk);
                    yylex.setScanAngleStringLiteralTokens(false);

                    if (!tk.newline()
                        && (tk.is(T_STRING_LITERAL) || tk.is(T_ANGLE_STRING_LITERAL))) {
                        scanResult.size = int(tk.length() - 2);
                        if (tk.is(T_STRING_LITERAL))
                            scanResult.flags = SC_LOCAL_INCLUDE_FLAG;
                        else
                            scanResult.flags = SC_GLOBAL_INCLUDE_FLAG;
                        scanResult.fileName = context.fileContent + tk.begin() + 1;
                        context.includedFiles.push_back(scanResult);
                    }
                }
            }
        } else if (tk.is(T_IDENTIFIER)) {
            if (scanForFileTags) {
                if (oldTk.is(T_IDENTIFIER) && tc.equals(oldTk, defineLiteral)) {
                    // Someone was clever and redefined Q_OBJECT or Q_PLUGIN_METADATA.
                    // Example: iplugin.h in Qt Creator.
                } else {
                    if (tc.equals(tk, qobjectLiteral) || tc.equals(tk, qgadgetLiteral)
                        || tc.equals(tk, qnamespaceLiteral)) {
                        context.hasQObjectMacro = true;
                    } else if (tc.equals(tk, pluginMetaDataLiteral)) {
                        context.hasPluginMetaDataMacro = true;
                    }
                    if (!scanForDependencies && context.hasQObjectMacro
                        && (context.hasPluginMetaDataMacro
                            || context.fileType == CppScannerContext::FT_CPP
                            || context.fileType == CppScannerContext::FT_OBJCPP))
                        break;
                }
            }
        }
        oldTk = tk;
        yylex(&tk);
    }
}

bool scanCppFile(
    CppScannerContext &context,
    QStringView filePath,
    std::string_view fileTags,
    bool scanForFileTags,
    bool scanForDependencies)
{
    context.fileName = filePath.toString();
    const QList<QByteArray> &tagList
        = QByteArray::fromRawData(fileTags.data(), fileTags.size()).split(',');
    if (tagList.contains("hpp"))
        context.fileType = CppScannerContext::FT_HPP;
    else if (tagList.contains("cpp"))
        context.fileType = CppScannerContext::FT_CPP;
    else if (tagList.contains("objcpp"))
        context.fileType = CppScannerContext::FT_OBJCPP;
    else
        context.fileType = CppScannerContext::FT_UNKNOWN;

    size_t mapl = 0;
#ifdef Q_OS_UNIX
    QString filePathS = context.fileName;

    context.fd = ::open(qPrintable(filePathS), O_RDONLY);
    if (context.fd == -1) {
        context.fd = 0;
        return false;
    }

    struct stat s
    {};
    int r = fstat(context.fd, &s);
    if (r != 0)
        return false;
    mapl = s.st_size;
    context.mapl = mapl;

    void *vmap = mmap(nullptr, s.st_size, PROT_READ, MAP_PRIVATE, context.fd, 0);
    if (vmap == MAP_FAILED) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
        return false;
#else
    context.file.setFileName(context.fileName);
    if (!context.file.open(QFile::ReadOnly))
        return false;

    uchar *vmap = context.file.map(0, context.file.size());
    mapl = context.file.size();
#endif
    if (!vmap)
        return false;

    context.fileContent = reinterpret_cast<char *>(vmap);

    // Check for UTF-8 Byte Order Mark (BOM). Skip if found.
    if (mapl >= 3 && context.fileContent[0] == char(0xef) && context.fileContent[1] == char(0xbb)
        && context.fileContent[2] == char(0xbf)) {
        context.fileContent += 3;
        mapl -= 3;
    }

    CPlusPlus::Lexer lex(context.fileContent, context.fileContent + mapl);
    doScanCppFile(context, lex, scanForFileTags, scanForDependencies);
    return true;
}

span<const std::string_view> additionalFileTags(const CppScannerContext &context)
{
    static const std::string_view thMocCpp[] = {"moc_cpp"};
    static const std::string_view thMocHpp[] = {"moc_hpp"};
    static const std::string_view thMocPluginHpp[] = {"moc_hpp_plugin"};
    static const std::string_view thMocPluginCpp[] = {"moc_cpp_plugin"};

    if (context.hasQObjectMacro) {
        switch (context.fileType) {
        case CppScannerContext::FT_CPP:
        case CppScannerContext::FT_OBJCPP:
            return {context.hasPluginMetaDataMacro ? thMocPluginCpp : thMocCpp, 1};
        case CppScannerContext::FT_HPP:
            return {context.hasPluginMetaDataMacro ? thMocPluginHpp : thMocHpp, 1};
        default:
            break;
        }
    }
    return {};
}

} // namespace qbs::Internal
