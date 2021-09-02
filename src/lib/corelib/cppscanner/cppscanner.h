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

#include <tools/qbs_export.h>
#include <tools/span.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <QtCore/qfile.h>
#endif

namespace qbs::Internal {

struct QBS_EXPORT ScanResult
{
    std::string_view fileName;
    int flags = 0;
};

struct QBS_EXPORT CppScannerContext
{
    enum FileType { FT_UNKNOWN, FT_HPP, FT_CPP, FT_CPPM, FT_C, FT_OBJC, FT_OBJCPP, FT_RC };

    CppScannerContext() = default;
    CppScannerContext(const CppScannerContext &other) = delete;
    CppScannerContext(CppScannerContext &&other) = delete;
    CppScannerContext &operator=(const CppScannerContext &other) = delete;
    CppScannerContext &operator=(CppScannerContext &&other) = delete;
    ~CppScannerContext()
    {
#ifdef Q_OS_UNIX
        if (vmap)
            munmap(vmap, mapl);
        if (fd)
            close(fd);
#endif
    }

#ifdef Q_OS_WIN
    QFile file;
#endif
#ifdef Q_OS_UNIX
    int fd{0};
    void *vmap{nullptr};
    size_t mapl{0};
#endif

    QString fileName;
    std::string_view fileContent;
    FileType fileType{FT_UNKNOWN};
    QList<ScanResult> includedFiles;
    QByteArray providesModule;
    bool isInterface{false};
    QList<QByteArray> requiresModules;
    bool hasQObjectMacro{false};
    bool hasPluginMetaDataMacro{false};
};

QBS_EXPORT bool scanCppFile(
    CppScannerContext &context,
    QStringView filePath,
    std::string_view fileTags,
    bool scanForFileTags,
    bool scanForDependencies);

span<const std::string_view> additionalFileTags(const CppScannerContext &context);

} // namespace qbs::Internal
