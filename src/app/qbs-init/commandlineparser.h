/****************************************************************************
**
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com).
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

#ifndef QBS_INIT_COMMANDLINEPARSER_H
#define QBS_INIT_COMMANDLINEPARSER_H

#include "inititem.h"

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class CommandLineParser
{
public:
    void parse(const QStringList &commandLine);

    QString projectName() const { return m_projectName; }
    QString version() const { return m_version; }
    QStringList depends() const { return m_depends; }
    qbs::InitItem::ItemType itemType() const { return m_itemType; }
    qbs::InitItem::Language language() const { return m_language; }

private:
    [[noreturn]] void throwError(const QString &message);
    void validateProjectName(const QString &name);
    qbs::InitItem::Language languageFromName(const QString &name);
    qbs::InitItem::ItemType itemTypeFromName(const QString &name);

    QString m_projectName;
    QString m_version;
    QStringList m_depends;
    qbs::InitItem::ItemType m_itemType = qbs::InitItem::ItemType::Application;
    qbs::InitItem::Language m_language = qbs::InitItem::Language::Cpp;
};

#endif // QBS_INIT_COMMANDLINEPARSER_H
