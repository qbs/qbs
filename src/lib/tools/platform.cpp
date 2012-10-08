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

#include <QCoreApplication>
#include <QDebug>
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
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    QString localSettingsPath = QDir::homePath() + "/.config/Nokia/qbs/platforms";
#elif defined(Q_OS_WIN)
    QString localSettingsPath;
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
