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
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    this->setWindowTitle(tr("Quick System Info"));
    this->setWindowIcon(QIcon::fromTheme("mx-qsi"));
    ui->widget->setEnabled(false);
    ui->textSysInfo->setWordWrapMode(QTextOption::NoWrap);
    ui->textSysInfo->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->textSysInfo->setPlainText(tr("Loading..."));
    resize(QGuiApplication::primaryScreen()->availableGeometry().size() * 0.6);
    connect(ui->textSysInfo, &QPlainTextEdit::customContextMenuRequested, this, &MainWindow::createmenu);
    // This fires the lengthy setup routine after the window is displayed.
    QTimer::singleShot(0, this, &MainWindow::setup);
}

MainWindow::~MainWindow() { delete ui; }

// setup versious items first time program runs
void MainWindow::setup()
{
    version = getVersion("quick-system-info-gui");
    buildcomboBoxCommand();
    QAction *copyreport = new QAction(this);
    copyreport->setShortcut(Qt::CTRL | Qt::Key_C);
    connect(copyreport, &QAction::triggered, this, &MainWindow::forumcopy);
    QAction *plaincopyaction = new QAction(this);
    connect(plaincopyaction, &QAction::triggered, this, &MainWindow::plaincopy);
    plaincopyaction->setShortcut(Qt::ALT | Qt::Key_C);
    QAction *savefileaction = new QAction(this);
    connect(savefileaction, &QAction::triggered, this, &MainWindow::on_pushSave_clicked);
    savefileaction->setShortcut(Qt::CTRL | Qt::Key_S);

    this->addAction(copyreport);
    this->addAction(plaincopyaction);
    this->addAction(savefileaction);

    ui->ButtonCopy->setDefault(true);
    ui->widget->setEnabled(true);
    QApplication::setOverrideCursor(QCursor());
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
    switch(ui->comboBoxCommand->currentIndex()){
      case 0:
        dialog.setDefaultSuffix("txt");
        dialog.setNameFilters({"*.txt"});
        dialog.selectFile("sysinfo.txt");
        break;

      case 1:
        dialog.setDefaultSuffix("txt");
        dialog.setNameFilters({"*.txt"});
        dialog.selectFile("apthistory.txt");
        break;

      default:
        dialog.setDefaultSuffix("log");
        dialog.setNameFilters({"*.log"});
        dialog.selectFile(ui->comboBoxCommand->currentText());
        break;
    }

    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec()) {
        QFile file(dialog.selectedFiles().at(0));
        bool ok = false;
        if (file.open(QFile::Truncate | QFile::WriteOnly)) {
            const QByteArray &text = ui->textSysInfo->toPlainText().toUtf8();
            ok = (file.write(text) == text.size());
            file.close();
        }
        if (ok)
            QMessageBox::information(this, windowTitle(), tr("System information saved."));
        else
            QMessageBox::critical(this, windowTitle(), tr("Could not save system information."));
    }
}

void MainWindow::on_ButtonCopy_clicked() { forumcopy(); }

void MainWindow::systeminfo()
{
    QString text = runCmd(QStringLiteral("/usr/bin/quick-system-info-mx -g")).output;
    text.remove("[code]");
    text.remove("[/code]");
    text.replace("http: /", "http:/");
    text.replace("https: /", "https:/");
    ui->textSysInfo->setPlainText(text.trimmed());
}

void MainWindow::apthistory()
{
    QString text = runCmd(QStringLiteral("zgrep -EH ' install | upgrade | purge | remove ' /var/log/dpkg* | cut -f2- -d: | sort -r | sed 's/ remove / remove  /;s/ purge / purge   /' | grep \"^\" ")).output;
    ui->textSysInfo->setPlainText(text.trimmed());
}

void MainWindow::buildcomboBoxCommand()
{
    QString logfilelist=runCmd("pkexec /usr/lib/quick-system-info-gui/qsig-lib-list list").output;
    QStringList logfiles = logfilelist.split("\n");
    QStringList logfilespost;


    QStringListIterator it(logfiles);
    while(it.hasNext()){
        QString i = it.next();
        i.remove("/var/log/");
        logfilespost.append(i);
    }

    logfilespost.sort(Qt::CaseInsensitive);
    logfilespost.prepend("apt " + tr("history"));
    logfilespost.prepend(tr("Quick System Info"));

    ui->comboBoxCommand->addItems(logfilespost);

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

void MainWindow::createmenu(QPoint pos)
{
    QMenu menu(this);
    forumcopyaction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy-symbolic")), tr("Copy for forum"), this);
    forumcopyaction->setShortcutVisibleInContextMenu(true);
    forumcopyaction->setShortcut(Qt::CTRL | Qt::Key_C);
    connect(forumcopyaction, &QAction::triggered, this, &MainWindow::forumcopy);
    plaincopyaction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy-symbolic")), tr("Plain text copy"), this);
    plaincopyaction->setShortcutVisibleInContextMenu(true);
    plaincopyaction->setShortcut(Qt::ALT | Qt::Key_C);
    connect(plaincopyaction, &QAction::triggered, this, &MainWindow::plaincopy);
    saveasfile = new QAction(QIcon::fromTheme(QStringLiteral("document-save")), tr("Save"), this);
    saveasfile->setShortcutVisibleInContextMenu(true);
    saveasfile->setShortcut(Qt::CTRL | Qt::Key_S);
    connect(saveasfile, &QAction::triggered, this, &MainWindow::on_pushSave_clicked);
    menu.addAction(forumcopyaction);
    menu.addAction(plaincopyaction);
    menu.addAction(saveasfile);

    menu.exec(ui->textSysInfo->mapToGlobal(pos));
    forumcopyaction->deleteLater();
    plaincopyaction->deleteLater();
    saveasfile->deleteLater();
}

void MainWindow::displaylog(const QString &logfile)
{
    QString text;
    QFile file("/var/log/" + logfile);
    if (QFileInfo("/var/log/" + logfile).permission(QFile::ReadOther)){
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(0, "error", file.errorString());
            return;
        }
        QTextStream in(&file);

        while(!in.atEnd()) {
            text = in.readAll();
        }

        file.close();
    } else {
        text = runCmd("pkexec /usr/lib/quick-system-info-gui/qsig-lib readadminfile /var/log/" + logfile).output;
    }

    ui->textSysInfo->setPlainText(text.trimmed());
}


void MainWindow::on_comboBoxCommand_currentIndexChanged(int index)
{
    switch(index){
      case 0:
        ui->textSysInfo->setPlainText(tr("Loading..."));
        systeminfo();
        break;

        case 1:
        apthistory();
        break;

        default:
        displaylog(ui->comboBoxCommand->currentText());
        break;
    }

}
