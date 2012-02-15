/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include <QtCore/qglobal.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

#if defined(Q_OS_WIN) && defined(Q_CC_MSVC)

#include <qt_windows.h>

static BOOL WINAPI consoleCtrlHandlerRoutine(__in  DWORD dwCtrlType)
{
    Q_UNUSED(dwCtrlType);
    QTimer::singleShot(0, qApp, SLOT(userInterrupt()));
    return TRUE;
}

void installCtrlCHandler()
{
    SetConsoleCtrlHandler(&consoleCtrlHandlerRoutine, TRUE);
}

#else

#include <csignal>

static void sigIntHandler(int sig)
{
    Q_UNUSED(sig);
    QTimer::singleShot(0, qApp, SLOT(userInterrupt()));
}

void installCtrlCHandler()
{
    signal(SIGINT, sigIntHandler);
}

#endif
