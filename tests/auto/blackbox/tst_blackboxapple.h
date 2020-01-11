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

#ifndef TST_BLACKBOXAPPLE_H
#define TST_BLACKBOXAPPLE_H

#include "tst_blackboxbase.h"

namespace qbs {
class Version;
} // namespace qbs

class TestBlackboxApple : public TestBlackboxBase
{
    Q_OBJECT

public:
    TestBlackboxApple();

public slots:
    void initTestCase() override;

private slots:
    void appleMultiConfig();
    void aggregateDependencyLinking();
    void assetCatalog();
    void assetCatalog_data();
    void assetCatalogsEmpty();
    void assetCatalogsMultiple();
    void bundleStructure();
    void bundleStructure_data();
    void deploymentTarget();
    void deploymentTarget_data();
    void dmg();
    void embedInfoPlist();
    void frameworkStructure();
    void iconset();
    void iconsetApp();
    void infoPlist();
    void objcArc();
    void overrideInfoPlist();
    void xcode();

private:
    QVariantMap findXcode(int *status = nullptr);
    qbs::Version findXcodeVersion();
};

#endif // TST_BLACKBOXAPPLE_H
