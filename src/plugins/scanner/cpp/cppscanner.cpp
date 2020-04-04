/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "../scanner.h"
#include "cpp_global.h"
#include "Lexer.h"

using namespace CPlusPlus;

#include <tools/qbspluginmanager.h>
#include <tools/scannerpluginmanager.h>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <QtCore/qfile.h>
#endif

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <cstring>
#include <memory>

struct ScanResult
{
    char *fileName = nullptr;
    int size = 0;
    int flags = 0;
};

struct Opaq
{
    enum FileType
    {
        FT_UNKNOWN, FT_HPP, FT_CPP, FT_C, FT_OBJC, FT_OBJCPP, FT_RC
    };

    Opaq()
        :
#ifdef Q_OS_UNIX
          fd(0),
          mapl(0),
#endif
          fileContent(nullptr),
          fileType(FT_UNKNOWN),
          hasQObjectMacro(false),
          hasPluginMetaDataMacro(false),
          currentResultIndex(0)
    {}

    ~Opaq()
    {
#ifdef Q_OS_UNIX
        if (fileContent)
            munmap(fileContent, mapl);
        if (fd)
            close(fd);
#endif
    }

#ifdef Q_OS_WIN
    QFile file;
#endif
#ifdef Q_OS_UNIX
    int fd;
    size_t mapl;
#endif

    QString fileName;
    char *fileContent;
    FileType fileType;
    QList<ScanResult> includedFiles;
    bool hasQObjectMacro;
    bool hasPluginMetaDataMacro;
    int currentResultIndex;
};

class TokenComparator
{
    const char * const m_fileContent;
public:
    TokenComparator(const char *fileContent)
        : m_fileContent(fileContent)
    {
    }

    bool equals(const Token &tk, const QLatin1String &literal) const
    {
        return static_cast<int>(tk.length()) == literal.size()
                && memcmp(m_fileContent + tk.begin(), literal.data(), literal.size()) == 0;
    }
};

static void scanCppFile(void *opaq, CPlusPlus::Lexer &yylex, bool scanForFileTags,
                        bool scanForDependencies)
{
    const QLatin1String includeLiteral("include");
    const QLatin1String importLiteral("import");
    const QLatin1String defineLiteral("define");
    const QLatin1String qobjectLiteral("Q_OBJECT");
    const QLatin1String qgadgetLiteral("Q_GADGET");
    const QLatin1String qnamespaceLiteral("Q_NAMESPACE");
    const QLatin1String pluginMetaDataLiteral("Q_PLUGIN_METADATA");
    const auto opaque = static_cast<Opaq *>(opaq);
    const TokenComparator tc(opaque->fileContent);
    Token tk;
    Token oldTk;
    ScanResult scanResult;

    yylex(&tk);

    while (tk.isNot(T_EOF_SYMBOL)) {
        if (tk.newline() && tk.is(T_POUND)) {
            yylex(&tk);

            if (scanForDependencies && !tk.newline() && tk.is(T_IDENTIFIER)) {
                if (tc.equals(tk, includeLiteral) || tc.equals(tk, importLiteral))
                {
                    yylex.setScanAngleStringLiteralTokens(true);
                    yylex(&tk);
                    yylex.setScanAngleStringLiteralTokens(false);

                    if (!tk.newline() && (tk.is(T_STRING_LITERAL) || tk.is(T_ANGLE_STRING_LITERAL))) {
                        scanResult.size = int(tk.length() - 2);
                        if (tk.is(T_STRING_LITERAL))
                            scanResult.flags = SC_LOCAL_INCLUDE_FLAG;
                        else
                            scanResult.flags = SC_GLOBAL_INCLUDE_FLAG;
                        scanResult.fileName = opaque->fileContent + tk.begin() + 1;
                        opaque->includedFiles.push_back(scanResult);
                    }
                }
            }
        } else if (tk.is(T_IDENTIFIER)) {
            if (scanForFileTags) {
                if (oldTk.is(T_IDENTIFIER) && tc.equals(oldTk, defineLiteral)) {
                    // Someone was clever and redefined Q_OBJECT or Q_PLUGIN_METADATA.
                    // Example: iplugin.h in Qt Creator.
                } else {
                    if (tc.equals(tk, qobjectLiteral) || tc.equals(tk, qgadgetLiteral)  ||
                        tc.equals(tk, qnamespaceLiteral))
                    {
                        opaque->hasQObjectMacro = true;
                    } else if (tc.equals(tk, pluginMetaDataLiteral))
                    {
                        opaque->hasPluginMetaDataMacro = true;
                    }
                    if (!scanForDependencies && opaque->hasQObjectMacro
                        && (opaque->hasPluginMetaDataMacro
                            || opaque->fileType == Opaq::FT_CPP
                            || opaque->fileType == Opaq::FT_OBJCPP))
                        break;
                }
            }

        }
        oldTk = tk;
        yylex(&tk);
    }
}

static void *openScanner(const unsigned short *filePath, const char *fileTags, int flags)
{
    std::unique_ptr<Opaq> opaque(new Opaq);
    opaque->fileName = QString::fromUtf16(filePath);
    const int fileTagsLength = static_cast<int>(std::strlen(fileTags));
    const QList<QByteArray> &tagList = QByteArray::fromRawData(fileTags, fileTagsLength).split(',');
    if (tagList.contains("hpp"))
        opaque->fileType = Opaq::FT_HPP;
    else if (tagList.contains("cpp"))
        opaque->fileType = Opaq::FT_CPP;
    else if (tagList.contains("objcpp"))
        opaque->fileType = Opaq::FT_OBJCPP;
    else
        opaque->fileType = Opaq::FT_UNKNOWN;

    size_t mapl = 0;
#ifdef Q_OS_UNIX
    QString filePathS = opaque->fileName;

    opaque->fd = open(qPrintable(filePathS), O_RDONLY);
    if (opaque->fd == -1) {
        opaque->fd = 0;
        return nullptr;
    }

    struct stat s{};
    int r = fstat(opaque->fd, &s);
    if (r != 0)
        return nullptr;
    mapl = s.st_size;
    opaque->mapl = mapl;

    void *vmap = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, opaque->fd, 0);
    if (vmap == MAP_FAILED) // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
        return nullptr;
#else
    opaque->file.setFileName(opaque->fileName);
    if (!opaque->file.open(QFile::ReadOnly))
        return nullptr;

    uchar *vmap = opaque->file.map(0, opaque->file.size());
    mapl = opaque->file.size();
#endif
    if (!vmap)
        return nullptr;

    opaque->fileContent = reinterpret_cast<char *>(vmap);

    // Check for UTF-8 Byte Order Mark (BOM). Skip if found.
    if (mapl >= 3
            && opaque->fileContent[0] == char(0xef)
            && opaque->fileContent[1] == char(0xbb)
            && opaque->fileContent[2] == char(0xbf)) {
        opaque->fileContent += 3;
        mapl -= 3;
    }

    CPlusPlus::Lexer lex(opaque->fileContent, opaque->fileContent + mapl);
    scanCppFile(opaque.get(), lex, flags & ScanForFileTagsFlag, flags & ScanForDependenciesFlag);
    return opaque.release();
}

static void closeScanner(void *ptr)
{
    const auto opaque = static_cast<Opaq *>(ptr);
    delete opaque;
}

static const char *next(void *opaq, int *size, int *flags)
{
    const auto opaque = static_cast<Opaq*>(opaq);
    if (opaque->currentResultIndex < opaque->includedFiles.size()) {
        const ScanResult &result = opaque->includedFiles.at(opaque->currentResultIndex);
        ++opaque->currentResultIndex;
        *size = result.size;
        *flags = result.flags;
        return result.fileName;
    }
    *size = 0;
    *flags = 0;
    return nullptr;
}

static const char **additionalFileTags(void *opaq, int *size)
{
    static const char *thMocCpp[] = { "moc_cpp" };
    static const char *thMocHpp[] = { "moc_hpp" };
    static const char *thMocPluginHpp[] = { "moc_hpp_plugin" };
    static const char *thMocPluginCpp[] = { "moc_cpp_plugin" };

    const auto opaque = static_cast<const Opaq*>(opaq);
    if (opaque->hasQObjectMacro) {
        *size = 1;
        switch (opaque->fileType) {
        case Opaq::FT_CPP:
        case Opaq::FT_OBJCPP:
            return opaque->hasPluginMetaDataMacro ? thMocPluginCpp : thMocCpp;
        case Opaq::FT_HPP:
            return opaque->hasPluginMetaDataMacro ? thMocPluginHpp : thMocHpp;
        default:
            break;
        }
    }
    *size = 0;
    return nullptr;
}

ScannerPlugin includeScanner =
{
    "include_scanner",
    "cpp,cpp_pch_src,c,c_pch_src,objcpp,objcpp_pch_src,objc,objc_pch_src,rc",
    openScanner,
    closeScanner,
    next,
    additionalFileTags,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin *cppScanners[] = { &includeScanner, NULL };

static void QbsCppScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerPlugins(cppScanners);
}

static void QbsCppScannerPluginUnload()
{
}

QBS_REGISTER_STATIC_PLUGIN(extern "C" CPPSCANNER_EXPORT, qbs_cpp_scanner,
                           QbsCppScannerPluginLoad, QbsCppScannerPluginUnload)

