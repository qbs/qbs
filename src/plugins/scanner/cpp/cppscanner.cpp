/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "../scanner.h"
#include "cpp_global.h"
#include "Lexer.h"

using namespace CPlusPlus;

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <QtCore/QFile>
#endif

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>
#include <QtCore/QLatin1Literal>

struct ScanResult
{
    char *fileName;
    unsigned int size;
    int flags;
};

struct Opaq
{
    enum FileType
    {
        FT_UNKNOWN, FT_HPP, FT_CPP
    };

    Opaq()
        :
#ifdef Q_OS_UNIX
          fd(0),
          mapl(0),
#endif
          fileContent(0),
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

static void scanCppFile(void *opaq, Lexer &yylex, bool scanForFileTags)
{
    const QLatin1Literal includeLiteral("include");
    const QLatin1Literal importLiteral("import");
    const QLatin1Literal qobjectLiteral("Q_OBJECT");
    const QLatin1Literal qgadgetLiteral("Q_GADGET");
    const QLatin1Literal pluginMetaDataLiteral("Q_PLUGIN_METADATA");
    Opaq *opaque = static_cast<Opaq *>(opaq);
    Token tk;
    ScanResult scanResult;

    yylex(&tk);

    while (tk.isNot(T_EOF_SYMBOL)) {
        if (tk.newline() && tk.is(T_POUND)) {
            yylex(&tk);

            if (!scanForFileTags && !tk.newline() && tk.is(T_IDENTIFIER)) {
                if ((static_cast<int>(tk.length()) >= includeLiteral.size()
                     && (strncmp(opaque->fileContent + tk.begin(), includeLiteral.data(), includeLiteral.size()) == 0))
                        || (static_cast<int>(tk.length()) >= importLiteral.size()
                            && (strncmp(opaque->fileContent + tk.begin(), importLiteral.data(), importLiteral.size()) == 0)))
                {
                    yylex.setScanAngleStringLiteralTokens(true);
                    yylex(&tk);
                    yylex.setScanAngleStringLiteralTokens(false);

                    if (!tk.newline() && (tk.is(T_STRING_LITERAL) || tk.is(T_ANGLE_STRING_LITERAL))) {
                        scanResult.size = tk.length() - 2;
                        if (tk.is(T_STRING_LITERAL))
                            scanResult.flags = SC_LOCAL_INCLUDE_FLAG;
                        else
                            scanResult.flags = SC_GLOBAL_INCLUDE_FLAG;
                        scanResult.fileName = opaque->fileContent + tk.begin() + 1;
                        opaque->includedFiles.append(scanResult);
                    }
                }
            }
        } else if (tk.is(T_IDENTIFIER)) {
            if (scanForFileTags) {
                const char *identifier = opaque->fileContent + tk.begin();
                if (strncmp(identifier, qobjectLiteral.data(), qobjectLiteral.size()) == 0
                    || strncmp(identifier, qgadgetLiteral.data(), qgadgetLiteral.size()) == 0)
                {
                    opaque->hasQObjectMacro = true;
                } else if (opaque->fileType == Opaq::FT_HPP
                           && strncmp(identifier, pluginMetaDataLiteral.data(),
                                      pluginMetaDataLiteral.size()) == 0)
                {
                    opaque->hasPluginMetaDataMacro = true;
                }
                if (opaque->hasQObjectMacro
                    && (opaque->fileType == Opaq::FT_CPP || opaque->hasPluginMetaDataMacro))
                    break;
            }

        }
        yylex(&tk);
    }
}

static void *openScanner(const unsigned short *filePath, char **fileTags, int numFileTags)
{
    QScopedPointer<Opaq> opaque(new Opaq);
    opaque->fileName = QString::fromUtf16(filePath);

    size_t mapl = 0;
#ifdef Q_OS_UNIX
    QString filePathS = opaque->fileName;

    opaque->fd = open(qPrintable(filePathS), O_RDONLY);
    if (opaque->fd == -1) {
        opaque->fd = 0;
        return 0;
    }

    struct stat s;
    int r = fstat(opaque->fd, &s);
    if (r != 0)
        return 0;
    mapl = s.st_size;
    opaque->mapl = mapl;

    void *vmap = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, opaque->fd, 0);
    if (vmap == MAP_FAILED)
        return 0;
#else
    opaque->file.setFileName(opaque->fileName);
    if (!opaque->file.open(QFile::ReadOnly))
        return 0;

    uchar *vmap = opaque->file.map(0, opaque->file.size());
    mapl = opaque->file.size();
#endif
    if (!vmap)
        return 0;

    for (int i=0; i < numFileTags; ++i) {
        const char *fileTag = fileTags[i];
        if (strncmp("cpp", fileTag, 3) == 0)
            opaque->fileType = Opaq::FT_CPP;
        else if (strncmp("hpp", fileTag, 3) == 0)
            opaque->fileType = Opaq::FT_HPP;
    }

    opaque->fileContent = reinterpret_cast<char *>(vmap);
    Lexer lex(opaque->fileContent, opaque->fileContent + mapl);
    const bool scanForFileTags = fileTags && numFileTags;
    scanCppFile(opaque.data(), lex, scanForFileTags);
    return static_cast<void *>(opaque.take());
}

static void closeScanner(void *ptr)
{
    Opaq *opaque = static_cast<Opaq *>(ptr);
    delete opaque;
}

static const char *next(void *opaq, int *size, int *flags)
{
    Opaq *opaque = static_cast<Opaq*>(opaq);
    if (opaque->currentResultIndex < opaque->includedFiles.count()) {
        const ScanResult &result = opaque->includedFiles.at(opaque->currentResultIndex);
        ++opaque->currentResultIndex;
        *size = result.size;
        *flags = result.flags;
        return result.fileName;
    }
    *size = 0;
    *flags = 0;
    return 0;
}

static const char **additionalFileTags(void *opaq, int *size)
{
    static const char *thMocCpp[] = { "moc_cpp" };
    static const char *thMocHpp[] = { "moc_hpp" };
    static const char *thMocPluginHpp[] = { "moc_plugin_hpp" };

    Opaq *opaque = static_cast<Opaq*>(opaq);
    if (opaque->hasQObjectMacro) {
        *size = 1;
        switch (opaque->fileType) {
        case Opaq::FT_CPP:
            return thMocCpp;
        case Opaq::FT_HPP:
            return opaque->hasPluginMetaDataMacro ? thMocPluginHpp : thMocHpp;
        default:
            break;
        }
    }
    *size = 0;
    return 0;
}

extern "C" {

ScannerPlugin hppScanner =
{
    "include_scanner",
    "hpp",
    openScanner,
    closeScanner,
    next,
    additionalFileTags,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin cppScanner =
{
    "include_scanner",
    "cpp",
    openScanner,
    closeScanner,
    next,
    additionalFileTags,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin cScanner =
{
    "include_scanner",
    "c",
    openScanner,
    closeScanner,
    next,
    0,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin objcppScanner =
{
    "include_scanner",
    "objcpp",
    openScanner,
    closeScanner,
    next,
    additionalFileTags,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin objcScanner =
{
    "include_scanner",
    "objc",
    openScanner,
    closeScanner,
    next,
    additionalFileTags,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin rcScanner =
{
    "include_scanner",
    "rc",
    openScanner,
    closeScanner,
    next,
    0,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies
};

ScannerPlugin *theScanners[] = {&hppScanner, &cppScanner, &cScanner, &objcppScanner, &objcScanner, &rcScanner, NULL};

CPPSCANNER_EXPORT ScannerPlugin **getScanners()
{
    return theScanners;
}

} // extern "C"
