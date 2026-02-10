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

#if defined(WIN32) || defined(_WIN32)
#define SCANNER_EXPORT __declspec(dllexport)
#else
#define SCANNER_EXPORT __attribute__((visibility("default")))
#endif

#include "../scanner.h"

#include <tools/qbspluginmanager.h>
#include <tools/scannerpluginmanager.h>

#include <QtCore/qglobal.h>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <QtCore/qfile.h>
#endif

#include <QtCore/qfileinfo.h>
#include <QtCore/qstring.h>
#include <QtCore/qxmlstream.h>

#include <memory>

struct QrcScannerFile
{
#ifdef Q_OS_UNIX
    int fd = 0;
    size_t mapLength = 0;
#else
    std::unique_ptr<QFile> file;
#endif

    char *mapData = nullptr;
    int fileSize = 0;

    QrcScannerFile() = default;
    QrcScannerFile(const QrcScannerFile &) = delete;
    QrcScannerFile &operator=(const QrcScannerFile &) = delete;
    ~QrcScannerFile() { close(); }

    bool open(const QString &filePath);
    void close();

    const char *data() const { return mapData; }
    int size() const { return fileSize; }
};

bool QrcScannerFile::open(const QString &filePath)
{
    close();

#ifdef Q_OS_UNIX
    fd = ::open(qPrintable(filePath), O_RDONLY);
    if (fd == -1) {
        fd = 0;
        return false;
    }

    struct stat s
    {};
    if (fstat(fd, &s) != 0)
        return false;

    fileSize = static_cast<int>(s.st_size);
    mapLength = s.st_size;

    void *map = mmap(nullptr, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == nullptr || map == MAP_FAILED)
        return false;

    mapData = static_cast<char *>(map);
#else
    file = std::make_unique<QFile>(filePath);
    if (!file->open(QFile::ReadOnly))
        return false;

    fileSize = file->size();
    uchar *map = file->map(0, fileSize);
    if (!map)
        return false;

    mapData = reinterpret_cast<char *>(map);
#endif
    return true;
}

void QrcScannerFile::close()
{
#ifdef Q_OS_UNIX
    if (mapData) {
        munmap(mapData, mapLength);
        mapData = nullptr;
    }
    if (fd) {
        ::close(fd);
        fd = 0;
    }
#else
    file.reset();
#endif
    fileSize = 0;
}

class QrcScannerPlugin : public ScannerPlugin
{
public:
    QString name() const override { return QStringLiteral("qt_qrc_scanner"); }
    QStringList scan(const QString &filePath, const char *fileTags, const QVariantMap &properties)
        const override;
    QStringList collectSearchPaths(
        const QVariantMap &properties, const QStringList &productBuildDirectories) const override;
};

QStringList QrcScannerPlugin::scan(
    const QString &filePath, const char *fileTags, const QVariantMap &properties) const
{
    Q_UNUSED(fileTags);
    Q_UNUSED(properties);

    QStringList results;

    QrcScannerFile context;
    if (!context.open(filePath))
        return results;

    const QString baseDir = QFileInfo(filePath).path();
    QXmlStreamReader xml(QByteArray::fromRawData(context.data(), context.size()));

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::StartElement
            && xml.name() == QLatin1String("file")) {
            QString fileRef = xml.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement);
            if (!fileRef.isEmpty()) {
                // QRC file references are always relative to the QRC file location
                const QString resolvedPath = baseDir + QLatin1Char('/') + fileRef;
                if (QFile::exists(resolvedPath))
                    fileRef = resolvedPath;
                results.append(fileRef);
            }
        }
    }

    return results;
}

QStringList QrcScannerPlugin::collectSearchPaths(
    const QVariantMap &properties, const QStringList &productBuildDirectories) const
{
    Q_UNUSED(properties);
    Q_UNUSED(productBuildDirectories);
    return {};
}

static void QbsQtScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerScanner(
        std::make_unique<QrcScannerPlugin>());
}

static void QbsQtScannerPluginUnload() {}

QBS_REGISTER_STATIC_PLUGIN(
    extern "C" SCANNER_EXPORT, qbs_qt_scanner, QbsQtScannerPluginLoad, QbsQtScannerPluginUnload)
