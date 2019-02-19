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
#include "consoleprogressobserver.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <iostream>

namespace qbs {

void ConsoleProgressObserver::initialize(const QString &task, int max)
{
    m_maximum = max;
    m_value = 0;
    m_percentage = 0;
    m_hashesPrinted = 0;
    std::cout << task.toLocal8Bit().constData() << ": 0%" << std::flush;
    setMaximum(max);
}

void ConsoleProgressObserver::setMaximum(int maximum)
{
    m_maximum = maximum;
    if (maximum == 0) {
        m_percentage = 100;
        updateProgressBarIfNecessary();
        writePercentageString();
        std::cout << std::endl;
    }
}

void ConsoleProgressObserver::setProgressValue(int value)
{
    if (value > m_maximum || value <= m_value)
        return; // TODO: Should be an assertion, but the executor currently breaks it.
    m_value = value;
    const int newPercentage = (100 * m_value) / m_maximum;
    if (newPercentage == m_percentage)
        return;
    eraseCurrentPercentageString();
    m_percentage = newPercentage;
    updateProgressBarIfNecessary();
    writePercentageString();
    if (m_value == m_maximum)
        std::cout << std::endl;
    else
        std::cout << std::flush;
}

void ConsoleProgressObserver::eraseCurrentPercentageString()
{
    const int charsToErase = m_percentage == 0 ? 2 : m_percentage < 10 ? 3 : 4;
    const QByteArray backspaceCommand(charsToErase, '\b');

    // Move cursor before the old percentage string.
    std::cout << backspaceCommand.constData();

    // Erase old percentage string.
    std::cout << QByteArray(charsToErase, ' ').constData();

    // Move cursor before the erased string.
    std::cout << backspaceCommand.constData();
}

void ConsoleProgressObserver::updateProgressBarIfNecessary()
{
    static const int TotalHashCount = 50; // Should fit on most terminals without a line break.
    const int hashesNeeded = (m_percentage * TotalHashCount) / 100;
    if (m_hashesPrinted < hashesNeeded) {
        std::cout << QByteArray(hashesNeeded - m_hashesPrinted, '#').constData();
        m_hashesPrinted = hashesNeeded;
    }
}

void ConsoleProgressObserver::writePercentageString()
{
    std::cout << QStringLiteral(" %1%").arg(m_percentage).toLocal8Bit().constData();
}

} // namespace qbs
