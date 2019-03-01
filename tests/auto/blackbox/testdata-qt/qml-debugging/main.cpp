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

#include <QtGlobal>
#include <QString>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QGuiApplication>
#include <QQmlApplicationEngine>
using Application = QGuiApplication;
#define AH_SO_THIS_IS_QT5
#else
#include <QApplication>
#include <QDeclarativeView>
#define AH_SO_THIS_IS_QT4
using Application = QApplication;
#endif

int main(int argc, char *argv[])
{
    Application app(argc, argv);
#ifdef AH_SO_THIS_IS_QT5
    QQmlApplicationEngine engine;
    engine.load(QUrl("blubb"));
#else
    QDeclarativeView view;
    view.setSource(QUrl("blubb"));
#endif

    return app.exec();
}
