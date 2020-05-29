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

#ifndef TST_BLACKBOXQT_H
#define TST_BLACKBOXQT_H

#include "tst_blackboxbase.h"

class TestBlackboxQt : public TestBlackboxBase
{
    Q_OBJECT

public:
    TestBlackboxQt();

private slots:
    void addQObjectMacroToGeneratedCppFile();
    void autoQrc();
    void cachedQml();
    void combinedMoc();
    void createProject();
    void dbusAdaptors();
    void dbusInterfaces();
    void forcedMoc();
    void includedMocCpp();
    void linkerVariant();
    void lrelease();
    void metaTypes_data();
    void metaTypes();
    void mixedBuildVariants();
    void mocAndCppCombining();
    void mocFlags();
    void mocSameFileName();
    void pkgconfig();
    void pluginMetaData();
    void pluginSupport_data();
    void pluginSupport();
    void qmlDebugging();
    void qobjectInObjectiveCpp();
    void qmlTypeRegistrar_data();
    void qmlTypeRegistrar();
    void qtKeywords();
    void quickCompiler();
    void qtScxml();
    void removeMocHeaderFromFileList();
    void staticQtPluginLinking();
    void trackAddMocInclude();
    void track_qobject_change();
    void track_qrc();
    void unmocable();
};

#endif // TST_BLACKBOXQT_H
