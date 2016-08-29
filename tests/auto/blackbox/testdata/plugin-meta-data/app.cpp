/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPluginLoader>
#include <QtDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QPluginLoader loader(QLatin1String("thePlugin"));
    const QJsonValue v = loader.metaData().value(QLatin1String("theKey"));
    if (!v.isArray()) {
        qDebug() << "value is" << v;
        return 1;
    }
    const QJsonArray a = v.toArray();
    if (a.count() != 1 || a.first() != QLatin1String("theValue")) {
        qDebug() << "value is" << v;
        return 1;
    }
    return 0;
}
