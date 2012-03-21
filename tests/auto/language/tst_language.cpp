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

#include <language/loader.h>
#include <QtTest/QtTest>

using namespace qbs;

class TestLanguage : public QObject
{
    Q_OBJECT
    Loader *loader;

    QHash<QString, ResolvedProduct::Ptr> productsFromProject(ResolvedProject::Ptr project)
    {
        QHash<QString, ResolvedProduct::Ptr> result;
        foreach (ResolvedProduct::Ptr product, project->products)
            result.insert(product->name, product);
        return result;
    }

private slots:
    void initTestCase()
    {
        loader = new Loader();
        loader->setSearchPaths(QStringList() << SRCDIR "../../../share/qbs");
    }

    void cleanupTestCase()
    {
        delete loader;
    }

    void productConditions()
    {
        bool exceptionCaught = false;
        try {
            loader->loadProject(SRCDIR "testdata/productconditions.qbs");
            Configuration::Ptr cfg(new Configuration);
            QFutureInterface<bool> futureInterface;
            ResolvedProject::Ptr project = loader->resolveProject("someBuildDirectory", cfg, futureInterface);
            QVERIFY(project);
            QHash<QString, ResolvedProduct::Ptr> products = productsFromProject(project);
            QCOMPARE(products.count(), 3);
            QVERIFY(products.value("product_no_condition"));
            QVERIFY(products.value("product_true_condition"));
            QVERIFY(products.value("product_condition_dependent_of_module"));
        }
        catch (Error &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
    }

};

QTEST_MAIN(TestLanguage)

#include "tst_language.moc"
