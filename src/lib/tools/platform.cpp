/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "platform.h"

#include <tools/fileinfo.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

#ifdef Q_OS_WIN
#   include <qt_windows.h>
#   ifndef _WIN32_IE
#       define _WIN32_IE 0x0400    // for MinGW
#   endif
#   include <shlobj.h>
#endif

namespace qbs {
using namespace Internal;

Platform::Platform(const QString &_name, const QString& configpath)
    : name(_name)
    , settings(configpath + "/setup.ini", QSettings::IniFormat)
{
}

QHash<QString, Platform::Ptr> Platform::platforms()
{
    QString localSettingsPath = configBaseDir();
    QHash<QString, Platform::Ptr> targets;
    QDirIterator i(localSettingsPath, QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        i.next();
        Platform *t = new Platform(i.fileName(), i.filePath());
        targets.insert(t->name, Platform::Ptr(t));
    }
    return targets;
}

QString Platform::configBaseDir()
{
    QString localSettingsPath;
#if defined(Q_OS_UNIX)
    localSettingsPath = QDir::homePath() + QLatin1String("/.config/QtProject/qbs/platforms");

    // TODO: Remove in 0.4.
    if (!FileInfo(localSettingsPath).exists()) {
        const QString oldSettingsPath = QDir::homePath()
                + QLatin1String("/.config/Nokia/qbs/platforms");
        if (FileInfo(oldSettingsPath).exists()) {
            QString error;
            if (!copyFileRecursion(oldSettingsPath, localSettingsPath, &error))
                qWarning("Failed to transfer settings: %s", qPrintable(error));
        }
    }

#elif defined(Q_OS_WIN)
    wchar_t wszPath[MAX_PATH];
    if (SHGetSpecialFolderPath(NULL, wszPath, CSIDL_APPDATA, TRUE))
        localSettingsPath = QString::fromUtf16(reinterpret_cast<ushort*>(wszPath)) + "/qbs/platforms";
#else
#error port me!
#endif
    return localSettingsPath;
}

QString Platform::internalKey()
{
    return QLatin1String("qbs-internal");
}

} // namespace qbs
