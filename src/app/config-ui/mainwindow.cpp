/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "settingsmodel.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_model = new SettingsModel(this);
    ui->treeView->setModel(m_model);
    connect(ui->treeView, SIGNAL(expanded(QModelIndex)), SLOT(adjustColumns()));
    adjustColumns();

    QMenu * const fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu * const viewMenu = menuBar()->addMenu(tr("&View"));

    QAction * const reloadAction = new QAction(tr("&Reload"), this);
    reloadAction->setShortcut(Qt::CTRL | Qt::Key_R);
    connect(reloadAction, SIGNAL(triggered()), SLOT(reloadSettings()));
    QAction * const expandAllAction = new QAction(tr("&Expand All"), this);
    expandAllAction->setShortcut(Qt::CTRL | Qt::Key_E);
    connect(expandAllAction, SIGNAL(triggered()), SLOT(expandAll()));
    QAction * const collapseAllAction = new QAction(tr("C&ollapse All"), this);
    collapseAllAction->setShortcut(Qt::CTRL | Qt::Key_O);
    connect(collapseAllAction, SIGNAL(triggered()), SLOT(collapseAll()));
    QAction * const exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(Qt::CTRL | Qt::Key_Q);
    connect(exitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    fileMenu->addAction(reloadAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    viewMenu->addAction(expandAllAction);
    viewMenu->addAction(collapseAllAction);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::adjustColumns()
{
    for (int column = 0; column < m_model->columnCount(); ++column)
        ui->treeView->resizeColumnToContents(column);
}

void MainWindow::expandAll()
{
    ui->treeView->expandAll();
    adjustColumns();
}

void MainWindow::collapseAll()
{
    ui->treeView->collapseAll();
    adjustColumns();
}

void MainWindow::reloadSettings()
{
    m_model->reload();
}
