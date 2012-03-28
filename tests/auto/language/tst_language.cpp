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
#include <tools/logger.h>
#include <tools/logsink.h>
#include <tools/scripttools.h>
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

    QVariant productPropertyValue(ResolvedProduct::Ptr product, QString propertyName)
    {
        QStringList propertyNameComponents = propertyName.split(QLatin1Char('.'));
        if (propertyNameComponents.count() > 1)
            propertyNameComponents.prepend(QLatin1String("modules"));
        return getConfigProperty(product->configuration->value(), propertyNameComponents);
    }

private slots:
    void initTestCase()
    {
        //Logger::instance().setLogSink(new ConsolePrintLogSink);
        //Logger::instance().setLevel(LoggerTrace);
        loader = new Loader();
        loader->setSearchPaths(QStringList()
                               << QLatin1String(SRCDIR "../../../share/qbs")
                               << QLatin1String(SRCDIR "testdata"));
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

    void propertiesBlocks_data()
    {
        QTest::addColumn<QString>("propertyName");
        QTest::addColumn<QStringList>("expectedValues");
        QTest::newRow("property_overwrite") << QString("dummy.defines") << QStringList("OVERWRITTEN");
        QTest::newRow("property_overwrite_no_outer") << QString("dummy.defines") << QStringList("OVERWRITTEN");
        QTest::newRow("property_append_to_outer") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO"));

        QTest::newRow("multiple_exclusive_properties") << QString("dummy.defines") << QStringList("OVERWRITTEN");
        QTest::newRow("multiple_exclusive_properties_no_outer") << QString("dummy.defines") << QStringList("OVERWRITTEN");
        QTest::newRow("multiple_exclusive_properties_append_to_outer") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO"));

        QTest::newRow("ambiguous_properties") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO") << QString("THREE"));
    }

    void propertiesBlocks()
    {
        QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
        QFETCH(QString, propertyName);
        QFETCH(QStringList, expectedValues);

        bool exceptionCaught = false;
        try {
            loader->loadProject(SRCDIR "testdata/propertiesblocks.qbs");
            Configuration::Ptr cfg(new Configuration);
            QFutureInterface<bool> futureInterface;
            ResolvedProject::Ptr project = loader->resolveProject("someBuildDirectory", cfg, futureInterface);
            QVERIFY(project);
            QHash<QString, ResolvedProduct::Ptr> products = productsFromProject(project);
            ResolvedProduct::Ptr product = products.value(productName);
            QVERIFY(product);
            QCOMPARE(product->name, productName);
            QVariant v = productPropertyValue(product, propertyName);
            QCOMPARE(v.toStringList(), expectedValues);
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
