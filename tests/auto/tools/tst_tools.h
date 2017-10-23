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

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE
class QTemporaryDir;
QT_END_NAMESPACE

namespace qbs {
class Settings;
}

class TestTools : public QObject
{
    Q_OBJECT

public:
    TestTools(qbs::Settings *settings);
    ~TestTools();

public slots:
   virtual void initTestCase();

private slots:
    void fileSaver();

    void fileCaseCheck();
    void testBuildConfigMerging();
    void testFileInfo();
    void testProcessNameByPid();
    void testProfiles();
    void testSettingsMigration();
    void testSettingsMigration_data();

    void set_operator_eq();
    void set_swap();
    void set_size();
    void set_capacity();
    void set_reserve();
    void set_clear();
    void set_remove();
    void set_contains();
    void set_containsSet();
    void set_begin();
    void set_end();
    void set_insert();
    void set_reverseIterators();
    void set_stlIterator();
    void set_stlMutableIterator();
    void set_setOperations();
    void set_makeSureTheComfortFunctionsCompile();
    void set_initializerList();
    void set_intersects();

    void stringutils_join();
    void stringutils_join_data();
    void stringutils_join_empty();
    void stringutils_join_char();
    void stringutils_join_char_data();
    void stringutils_startsWith();
    void stringutils_endsWith();
    void stringutils_trimmed();

private:
    QString setupSettingsDir1();
    QString setupSettingsDir2();
    QString setupSettingsDir3();

    qbs::Settings * const m_settings;
    QList<QTemporaryDir *> m_tmpDirs;

    const QString testDataDir;
};
