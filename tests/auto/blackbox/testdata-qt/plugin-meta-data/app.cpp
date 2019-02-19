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
#include <QStaticPlugin>
#include <QtDebug>

#ifdef QT_STATIC
Q_IMPORT_PLUGIN(ThePlugin)
#endif

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QJsonObject metaData;
    for (const QStaticPlugin &p : QPluginLoader::staticPlugins()) {
        const QJsonObject &md = p.metaData();
        if (md.value("className") == "ThePlugin") {
            metaData = md;
            break;
        }
    }
#ifdef QT_STATIC
    if (metaData.isEmpty()) {
        qDebug() << "no static metadata";
        return 1;
    }
#else
    if (!metaData.isEmpty()) {
        qDebug() << "static metadata";
        return 1;
    }
#endif
    if (metaData.isEmpty())
        metaData = QPluginLoader("thePlugin").metaData();
    const QJsonValue v = metaData.value(QStringLiteral("theKey"));
    if (!v.isArray()) {
        qDebug() << "value is" << v;
        return 1;
    }
    const QJsonArray a = v.toArray();
    if (a.size() != 1 || a.first() != QLatin1String("theValue")) {
        qDebug() << "value is" << v;
        return 1;
    }
    const QJsonValue v2 = metaData.value(QStringLiteral("MetaData")).toObject()
            .value(QStringLiteral("theOtherKey"));
    if (v2.toString() != QLatin1String("theOtherValue")) {
        qDebug() << "metadata:" << metaData;
        return 1;
    }
    qDebug() << "all ok!";
    return 0;
}
