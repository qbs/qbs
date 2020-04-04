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
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <tools/settingsmodel.h>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>

#include <QtGui/qevent.h>
#include <QtGui/qkeysequence.h>

#include <QtWidgets/qaction.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qstyleditemdelegate.h>

namespace {

class TrimValidator : public QValidator
{
public:
    explicit TrimValidator(QObject *parent = nullptr) : QValidator(parent) {}

public: // QValidator interface
    State validate(QString &input, int &pos) const override
    {
        Q_UNUSED(pos);
        if (input.startsWith(QLatin1Char(' ')) || input.endsWith(QLatin1Char(' ')))
            return State::Intermediate;
        return State::Acceptable;
    }

    void fixup(QString &input) const override
    {
        input = input.trimmed();
    }
};

class SettingsItemDelegate: public QStyledItemDelegate
{
public:
    explicit SettingsItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        const auto editor = QStyledItemDelegate::createEditor(parent, option, index);
        const auto lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
            lineEdit->setValidator(new TrimValidator(lineEdit));
        return editor;
    }
};

} // namespace

MainWindow::MainWindow(const QString &settingsDir, qbs::Settings::Scope scope, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_model = new qbs::SettingsModel(settingsDir, scope, this);
    ui->treeView->setModel(m_model);
    ui->treeView->setItemDelegate(new SettingsItemDelegate(ui->treeView));
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QTreeView::expanded, this, &MainWindow::adjustColumns);
    connect(ui->treeView, &QWidget::customContextMenuRequested,
            this, &MainWindow::provideContextMenu);
    adjustColumns();

    QMenu * const fileMenu = menuBar()->addMenu(tr("&File"));
    QMenu * const viewMenu = menuBar()->addMenu(tr("&View"));

    const auto reloadAction = new QAction(tr("&Reload"), this);
    reloadAction->setShortcut(QKeySequence::Refresh);
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadSettings);
    const auto saveAction = new QAction(tr("&Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveSettings);
    const auto expandAllAction = new QAction(tr("&Expand All"), this);
    expandAllAction->setShortcut(int(Qt::CTRL) | int(Qt::Key_E));
    connect(expandAllAction, &QAction::triggered, this, &MainWindow::expandAll);
    const auto collapseAllAction = new QAction(tr("C&ollapse All"), this);
    collapseAllAction->setShortcut(int(Qt::CTRL) | int(Qt::Key_O));
    connect(collapseAllAction, &QAction::triggered, this, &MainWindow::collapseAll);
    const auto exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setMenuRole(QAction::QuitRole);
    connect(exitAction, &QAction::triggered, this, &MainWindow::exit);

    fileMenu->addAction(reloadAction);
    fileMenu->addAction(saveAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    viewMenu->addAction(expandAllAction);
    viewMenu->addAction(collapseAllAction);

    ui->treeView->installEventFilter(this);
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
    if (m_model->hasUnsavedChanges()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(this,
                tr("Unsaved Changes"),
                tr("You have unsaved changes. Do you want to discard them?"));
        if (answer != QMessageBox::Yes)
            return;
    }
    m_model->reload();
}

void MainWindow::saveSettings()
{
    m_model->save();
}

void MainWindow::exit()
{
    if (m_model->hasUnsavedChanges()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(this,
                tr("Unsaved Changes"),
                tr("You have unsaved changes. Do you want to save them now?"));
        if (answer == QMessageBox::Yes)
            m_model->save();
    }
    qApp->quit();
}

void MainWindow::provideContextMenu(const QPoint &pos)
{
    const QModelIndex index = ui->treeView->indexAt(pos);
    if (index.isValid() && index.column() != m_model->keyColumn())
        return;
    const QString settingsKey = m_model->data(index).toString();

    QMenu contextMenu;
    QAction addKeyAction(this);
    QAction removeKeyAction(this);
    if (index.isValid()) {
        addKeyAction.setText(tr("Add new key below '%1'").arg(settingsKey));
        removeKeyAction.setText(tr("Remove key '%1' and all its sub-keys").arg(settingsKey));
        contextMenu.addAction(&addKeyAction);
        contextMenu.addAction(&removeKeyAction);
    } else {
        addKeyAction.setText(tr("Add new top-level key"));
        contextMenu.addAction(&addKeyAction);
    }

    const QAction *action = contextMenu.exec(ui->treeView->mapToGlobal(pos));
    if (action == &addKeyAction)
        m_model->addNewKey(index);
    else if (action == &removeKeyAction)
        m_model->removeKey(index);
}

extern "C" void qt_macos_forceTransformProcessToForegroundApplicationAndActivate();

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (ui->treeView->hasFocus() && event->type() == QEvent::KeyPress) {
        const auto keyEvent = static_cast<const QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Delete)) {
            const QModelIndexList indexes = ui->treeView->selectionModel()->selectedRows();
            if (indexes.size() == 1) {
                const QModelIndex index = indexes.front();
                if (index.isValid()) {
                    m_model->removeKey(index);
                    return true;
                }
            }
        }
    }

    if (event->type() == QEvent::WindowActivate) {
        // Effectively delay the foreground process transformation from QApplication construction to
        // when the UI is shown - this prevents the application icon from popping up in the Dock
        // when running `qbs help`, and QCoreApplication::arguments() requires the application
        // object to be constructed, so it is not easily worked around
    #if defined(Q_OS_MACOS) || defined(Q_OS_OSX)
        qt_macos_forceTransformProcessToForegroundApplicationAndActivate();
    #endif
    }


    return QMainWindow::eventFilter(watched, event);
}
