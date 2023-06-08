/**********************************************************************
 *  mainwindow.cpp
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Dolphin Oracle, AK-47, Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of mx-welcome.
 *
 * mx-welcome is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mx-welcome is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mx-welcome.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <cassert>
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QPoint>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>

#include "QClipboard"
#include "about.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    lockGUI(true);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    ui->textSysInfo->setWordWrapMode(QTextOption::NoWrap);
    ui->textSysInfo->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->textSysInfo->setPlainText(tr("Loading..."));
    resize(QGuiApplication::primaryScreen()->availableGeometry().size() * 0.6);
    ui->listInfo->setContextMenuPolicy(Qt::ActionsContextMenu);
    defaultMatches = arg_parser.positionalArguments();
    // This fires the lengthy setup routine after the window is displayed.
    QTimer::singleShot(0, this, &MainWindow::setup);
}

MainWindow::~MainWindow() { delete ui; }

// setup versious items first time program runs
void MainWindow::setup()
{
    version = getVersion("quick-system-info-gui");

    // Log text box shortcuts and context menu
    QAction *forumcopyaction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy-symbolic")),
        tr("Copy for forum"), this);
    forumcopyaction->setShortcutVisibleInContextMenu(true);
    forumcopyaction->setShortcut(Qt::CTRL | Qt::Key_C);
    connect(forumcopyaction, &QAction::triggered, this, &MainWindow::forumcopy);
    QAction *plaincopyaction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy-symbolic")),
        tr("Plain text copy"), this);
    plaincopyaction->setShortcutVisibleInContextMenu(true);
    plaincopyaction->setShortcut(Qt::ALT | Qt::Key_C);
    connect(plaincopyaction, &QAction::triggered, this, &MainWindow::plaincopy);
    QAction *saveasfile = new QAction(QIcon::fromTheme(QStringLiteral("document-save")),
        tr("Save"), this);
    saveasfile->setShortcutVisibleInContextMenu(true);
    saveasfile->setShortcut(Qt::CTRL | Qt::Key_S);
    connect(saveasfile, &QAction::triggered, this, &MainWindow::on_pushSave_clicked);

    ui->textSysInfo->addAction(forumcopyaction);
    ui->textSysInfo->addAction(plaincopyaction);
    ui->textSysInfo->addAction(saveasfile);

    // Info list shortcuts and context menu.
    QAction *selall = new QAction(QIcon::fromTheme(QStringLiteral("edit-select-all")),
        tr("Select &All"), this);
    selall->setShortcut(Qt::CTRL | Qt::Key_A);
    selall->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    selall->setShortcutVisibleInContextMenu(true);
    connect(selall, &QAction::triggered, this, &MainWindow::listSelectAll);
    QAction *seldef = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear-all")),
        tr("Reset Selection"), this);
    seldef->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    seldef->setShortcutVisibleInContextMenu(true);
    connect(seldef, &QAction::triggered, this, &MainWindow::listSelectDefault);
    actionMultiSave = new QAction(QIcon::fromTheme(QStringLiteral("document-save")), QString(), this);
    actionMultiSave->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    actionMultiSave->setShortcutVisibleInContextMenu(true);
    connect(actionMultiSave, &QAction::triggered, this, &MainWindow::on_pushMultiSave_clicked);

    ui->listInfo->addAction(selall);
    ui->listInfo->addAction(seldef);
    ui->listInfo->addAction(actionMultiSave);

    buildInfoList();
    ui->listInfo->setCurrentRow(0);

    ui->ButtonCopy->setDefault(true);
    lockGUI(false);
}

void MainWindow::lockGUI(bool lock)
{
    if (ui->widget->isEnabled() == lock) {
        ui->widget->setDisabled(lock);
        if (lock) QApplication::setOverrideCursor(Qt::WaitCursor);
        else {
            ui->listInfo->setFocus();
            QApplication::restoreOverrideCursor();
        }
    }
}

// Util function for getting bash command output and error code
Result MainWindow::runCmd(const QString &cmd)
{
    QEventLoop loop;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    proc.start("/bin/bash", QStringList() << "-c" << cmd);
    loop.exec();
    return {proc.exitCode(), proc.readAll().trimmed()};
}

// Get version of the program
QString MainWindow::getVersion(const QString &name) { return runCmd("dpkg-query -f '${Version}' -W " + name).output; }

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(
        tr("About Quick-System-Info-gui"),
        "<p align=\"center\"><b><h2>" + tr("Quick System Info") + "</h2></b></p><p align=\"center\">" + tr("Version: ")
            + VERSION + "</p><p align=\"center\"><h3>" + tr("Program for displaying a quick system info report")
            + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
              "<p align=\"center\">"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        QStringLiteral("/usr/share/doc/quick-system-info-gui/license.html"), tr("License").arg(this->windowTitle()));
    this->show();
}

void MainWindow::on_pushSave_clicked()
{
    QFileDialog dialog(this, tr("Save System Information"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    QListWidgetItem *item = ui->listInfo->item(ui->listInfo->currentRow());
    assert(item != nullptr);
    const QString &selname = item->data(Qt::UserRole).toString();
    const QString &ext = QFileInfo(selname).suffix();
    dialog.setDefaultSuffix(ext);
    dialog.setNameFilter("*."+ext);
    dialog.selectFile(selname);

    if (dialog.exec()) {
        lockGUI(true);
        const QString selpath = dialog.selectedFiles().at(0);
        bool ok = false;
        QFile file(selpath);
        if (file.open(QFile::Truncate | QFile::WriteOnly)) {
            const QByteArray &text = ui->textSysInfo->toPlainText().toUtf8();
            ok = (file.write(text) != text.size());
            file.close();
        }
        if (ok) {
            QMessageBox::information(this, windowTitle(), tr("System information saved."));
        } else {
            QMessageBox::critical(this, windowTitle(), tr("Could not save system information."));
        }
        lockGUI(false);
    }
}
void MainWindow::on_pushMultiSave_clicked()
{
    QFileDialog dialog(this, tr("Save System Information"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec()) {
        lockGUI(true);
        const QString selpath = dialog.selectedFiles().at(0);
        bool ok = true;
        for (int row = 0; row < ui->listInfo->count(); ++row) {
            const QListWidgetItem *item = ui->listInfo->item(row);
            assert(item != nullptr);
            if (item->checkState() != Qt::Checked) continue;

            QFile file(selpath + '/' + item->data(Qt::UserRole).toString());
            if (file.open(QFile::Truncate | QFile::WriteOnly)) {
                QByteArray contents;
                switch(row) {
                    case 0: contents = systeminfo().toUtf8(); break;
                    case 1: contents = apthistory().toUtf8(); break;
                    default: contents = readlog(item->text()).toUtf8(); break;
                }
                if (file.write(contents) != contents.size()) ok = false;
                file.close();
            }
        }
        if (ok) {
            QMessageBox::information(this, windowTitle(), tr("System information saved."));
        } else {
            QMessageBox::critical(this, windowTitle(), tr("Could not save system information."));
        }
        lockGUI(false);
    }
}

void MainWindow::on_ButtonCopy_clicked() { forumcopy(); }

QString MainWindow::systeminfo()
{
    QString text = runCmd(QStringLiteral("/usr/bin/quick-system-info-mx -g")).output;
    text.remove("[code]");
    text.remove("[/code]");
    text.replace("http: /", "http:/");
    text.replace("https: /", "https:/");
    return text.trimmed();
}

QString MainWindow::apthistory()
{
    return runCmd(QStringLiteral("zgrep -EH ' install | upgrade | purge | remove ' /var/log/dpkg*"
        " | cut -f2- -d: | sort -r | sed 's/ remove / remove  /;s/ purge / purge   /'"
        " | grep \"^\" ")).output.trimmed();
}

void MainWindow::buildInfoList()
{
    QString logfilelist=runCmd("pkexec /usr/lib/quick-system-info-gui/qsig-lib-list list").output;
    QStringList logfiles = logfilelist.split("\n");
    ui->listInfo->blockSignals(true);
    ui->listInfo->clear();

    // Populate with log files and sort.
    for(QString &logfile : logfiles) {
        QFileInfo qfi(logfile);
        logfile.remove("/var/log/");
        auto *item = new QListWidgetItem(logfile, ui->listInfo);
        item->setData(Qt::UserRole, logfile.replace('/','+'));
        // Italics for log files that require root to read.
        if (!qfi.permission(QFile::ReadOther)) {
            QFont ifont = item->font();
            ifont.setItalic(true);
            item->setFont(ifont);
        }
    }
    ui->listInfo->sortItems(); // Sort current list before adding special items.

    // Special treatment for QSI because of how important it is.
    QListWidgetItem *item = new QListWidgetItem(tr("Quick System Info"));
    QFont ifont = item->font();
    ifont.setBold(true);
    item->setFont(ifont);
    item->setData(Qt::UserRole, "sysinfo.txt");
    ui->listInfo->insertItem(0, item);
    // Special apt history info
    item = new QListWidgetItem("apt " + tr("history"));
    item->setData(Qt::UserRole, "apthistory.txt");
    ui->listInfo->insertItem(1, item);

    listSelectDefault();
    ui->listInfo->blockSignals(false);
    on_listInfo_itemChanged(); // Set up multi buttons.

    // Resize the splitter according to the new contents
    QApplication::processEvents(); // Allow the scroll bar to materialise
    QList<int> sizes = ui->splitter->sizes();
    if(sizes.count() > 1) {
        const int total = sizes.at(0) + sizes.at(1);
        const int shint = ui->listInfo->sizeHintForColumn(0)
            + (sizes.at(0) - ui->listInfo->viewport()->contentsRect().width());
        sizes[0] = shint;
        sizes[1] = total - shint;
        ui->splitter->setSizes(sizes);
    }
}

void MainWindow::on_ButtonHelp_clicked()
{
    QString url = QStringLiteral("file:///usr/share/doc/quick-system-info-gui/quick-system-info-gui.html");

    displayDoc(url, tr("%1 Help").arg(tr("Quick System Info (gui)")));
}

void MainWindow::forumcopy()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = ui->textSysInfo->textCursor().selectedText();
    text.replace(QChar(0x2029), "\n");
    if (text.isEmpty()) {
        text = ui->textSysInfo->toPlainText();
    }
    clipboard->setText("[CODE]" + text + "[/CODE]");
}

void MainWindow::plaincopy()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = ui->textSysInfo->textCursor().selectedText();
    text.replace(QChar(0x2029), "\n");
    if (text.isEmpty()) {
        text = ui->textSysInfo->toPlainText();
    }
    clipboard->setText(text);
}

QString MainWindow::readlog(const QString &logfile)
{
    QString text;
    QFile file("/var/log/" + logfile);
    if (QFileInfo("/var/log/" + logfile).permission(QFile::ReadOther)){
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(0, "error", file.errorString());
            return text;
        }
        QTextStream in(&file);

        while(!in.atEnd()) {
            text += in.readAll();
        }

        file.close();
    } else {
        text = runCmd("pkexec /usr/lib/quick-system-info-gui/qsig-lib readadminfile /var/log/" + logfile).output;
    }

    return text.trimmed();
}

// The currentRowchanged() signal occurs before the selection change is displayed.
void MainWindow::on_listInfo_itemSelectionChanged()
{
    lockGUI(true);
    ui->textSysInfo->setPlainText(tr("Loading..."));

    const int selrow = ui->listInfo->currentRow();
    switch(selrow){
    case 0:
        ui->textSysInfo->setPlainText(systeminfo());
        break;

    case 1:
        ui->textSysInfo->setPlainText(apthistory());
        break;

    default:
        ui->textSysInfo->setPlainText(readlog(ui->listInfo->item(selrow)->text()));
        break;
    }

    lockGUI(false);
}

void MainWindow::on_listInfo_itemChanged()
{
    int nchecked = 0;
    for(int row = 0; row < ui->listInfo->count(); ++row) {
        if (ui->listInfo->item(row)->checkState() == Qt::Checked) {
            ++nchecked;
        }
    }
    const QString ctltext = tr("Save Selected (×%1)").arg(nchecked);
    ui->pushMultiSave->setEnabled(nchecked > 0);
    ui->pushMultiSave->setText(ctltext);
    assert(actionMultiSave != nullptr);
    actionMultiSave->setEnabled(nchecked > 0);
    actionMultiSave->setText(ctltext);
}

// List checkbox selection presets
void MainWindow::listSelectAll()
{
    for(int row = 0; row < ui->listInfo->count(); ++row) {
        ui->listInfo->item(row)->setCheckState(Qt::Checked);
    }
}
void MainWindow::listSelectDefault()
{
    const int lcount = ui->listInfo->count();
    if (lcount > 0) ui->listInfo->item(0)->setCheckState(Qt::Checked);
    for(int row = 1; row < lcount; ++row) {
        ui->listInfo->item(row)->setCheckState(Qt::Unchecked);
    }
    // Check any items that match what is specified on the command line.
    for(const QString &match : qAsConst(defaultMatches)) {
        QList<QListWidgetItem *> matched = ui->listInfo->findItems(match, Qt::MatchWildcard);
        for(QListWidgetItem *item : matched) {
            item->setCheckState(Qt::Checked);
        }
    }
}
