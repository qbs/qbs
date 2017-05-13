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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <QtCore/qfile.h>
#endif

#include <QtCore/qscopedpointer.h>
#include <QtCore/qstring.h>
#include <QtCore/qxmlstream.h>

struct OpaqQrc
{
#ifdef Q_OS_UNIX
    int fd;
    int mapl;
#else
    QFile *file;
#endif

    char *map;
    QXmlStreamReader *xml;
    QByteArray current;
    OpaqQrc()
#ifdef Q_OS_UNIX
        : fd (0),
#else
        : file(0),
#endif
          map(0),
          xml(0)
    {}

    ~OpaqQrc()
    {
#ifdef Q_OS_UNIX
        if (map)
            munmap (map, mapl);
        if (fd)
            close (fd);
#else
        delete file;
#endif
        delete xml;
    }
};

static void *openScannerQrc(const unsigned short *filePath, const char *fileTags, int flags)
{
    Q_UNUSED(flags);
    Q_UNUSED(fileTags);
    QScopedPointer<OpaqQrc> opaque(new OpaqQrc);

#ifdef Q_OS_UNIX
    QString filePathS = QString::fromUtf16(filePath);
    opaque->fd = open(qPrintable(filePathS), O_RDONLY);
    if (opaque->fd == -1) {
        opaque->fd = 0;
        return 0;
    }

    struct stat s;
    int r = fstat(opaque->fd, &s);
    if (r != 0)
        return 0;
    opaque->mapl = s.st_size;

    void *map = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, opaque->fd, 0);
    if (map == 0)
        return 0;
#else
    opaque->file = new QFile(QString::fromUtf16(filePath));
    if (!opaque->file->open(QFile::ReadOnly))
        return 0;

    uchar *map = opaque->file->map(0, opaque->file->size());
    if (!map)
        return 0;
#endif

    opaque->map = reinterpret_cast<char *>(map);
    opaque->xml = new QXmlStreamReader(opaque->map);

    return static_cast<void *>(opaque.take());
}

static void closeScannerQrc(void *ptr)
{
    OpaqQrc *opaque = static_cast<OpaqQrc *>(ptr);
    delete opaque;
}

static const char *nextQrc(void *opaq, int *size, int *flags)
{
    OpaqQrc *o= static_cast<OpaqQrc *>(opaq);
    while (!o->xml->atEnd()) {
        o->xml->readNext();
        switch (o->xml->tokenType()) {
            case QXmlStreamReader::StartElement:
                if (o->xml->name() == QLatin1String("file")) {
                    o->current = o->xml->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).toUtf8();
                    *flags = SC_LOCAL_INCLUDE_FLAG;
                    *size = o->current.size();
                    return o->current.data();
                }
                break;
            case QXmlStreamReader::EndDocument:
                return 0;
            default:
                break;
        }
    }
    return 0;
}

static const char **additionalFileTagsQrc(void *, int *size)
{
    *size = 0;
    return 0;
}

ScannerPlugin qrcScanner =
{
    "qt_qrc_scanner",
    "qrc",
    openScannerQrc,
    closeScannerQrc,
    nextQrc,
    additionalFileTagsQrc,
    NoScannerFlags
};

ScannerPlugin *qtScanners[] = {&qrcScanner, NULL};

static void QbsQtScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerPlugins(qtScanners);
}

static void QbsQtScannerPluginUnload()
{
}

QBS_REGISTER_STATIC_PLUGIN(extern "C" SCANNER_EXPORT, QbsQtScannerPlugin,
                           QbsQtScannerPluginLoad, QbsQtScannerPluginUnload)
