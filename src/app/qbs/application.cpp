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

#include "application.h"

#include "commandlinefrontend.h"
#include "ctrlchandler.h"

namespace qbs {

Application::Application(int &argc, char **argv)
    : QCoreApplication(argc, argv), m_clFrontend(nullptr), m_canceled(false)
{
    setApplicationName(QStringLiteral("qbs"));
    setOrganizationName(QStringLiteral("QtProject"));
    setOrganizationDomain(QStringLiteral("qt-project.org"));
}

Application *Application::instance()
{
    return qobject_cast<Application *>(QCoreApplication::instance());
}

void Application::setCommandLineFrontend(CommandLineFrontend *clFrontend)
{
    installCtrlCHandler();
    m_clFrontend = clFrontend;
}

/**
 * Interrupt the application. This is directly called from a signal handler.
 */
void Application::userInterrupt()
{
    if (m_canceled)
        return;
    Q_ASSERT(m_clFrontend);
    m_canceled = true;
    m_clFrontend->cancel();
}

} // namespace qbs
