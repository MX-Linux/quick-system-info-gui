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
#include <ctime>
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QClipboard>

#include <archive.h>
#include <archive_entry.h>

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
    // Allow user-friendly match strings.
    for(QString &match : defaultMatches) {
        if (!match.contains('.')) match.append(".txt");
    }

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
        tr("Save text..."), this);
    saveasfile->setShortcutVisibleInContextMenu(true);
    saveasfile->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
    connect(saveasfile, &QAction::triggered, this, &MainWindow::on_pushSaveText_clicked);

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
        tr("Revert Selection"), this);
    seldef->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    seldef->setShortcutVisibleInContextMenu(true);
    connect(seldef, &QAction::triggered, this, &MainWindow::listSelectDefault);
    actionSave = new QAction(QIcon::fromTheme(QStringLiteral("document-save")),
        tr("&Save..."), this);
    actionSave->setShortcut(Qt::CTRL | Qt::Key_S);
    actionSave->setShortcutVisibleInContextMenu(true);
    connect(actionSave, &QAction::triggered, this, &MainWindow::on_pushSave_clicked);

    ui->listInfo->addAction(selall);
    ui->listInfo->addAction(seldef);
    ui->listInfo->addAction(actionSave);

    ui->splitter->handle(1)->installEventFilter(this);
    buildInfoList();
    ui->listInfo->setCurrentRow(0);

    connect(ui->ButtonCopy, &QPushButton::clicked, this, &MainWindow::forumcopy);
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
Result MainWindow::runCmd(const QString &cmd, const QString *input)
{
    QEventLoop loop;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    proc.start("/bin/bash", {"-c", cmd});
    if (input && !input->isEmpty()) proc.write(input->toUtf8());
    proc.closeWriteChannel();
    loop.exec();
    return {proc.exitCode(), proc.readAll().trimmed()};
}

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

void MainWindow::on_pushSaveText_clicked()
{
    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    QListWidgetItem *item = ui->listInfo->item(ui->listInfo->currentRow());
    assert(item != nullptr);
    const QStringList filters({"text/plain", "text/x-log", "application/octet-stream"});
    dialog.setMimeTypeFilters(filters); // Segmentation fault when using init list directly.
    dialog.setDefaultSuffix("txt");
    dialog.setDirectory(getenv("HOME")); // Must be done before selectFile()
    dialog.selectFile(item->data(Qt::UserRole).toString().replace('/','+'));
    if (!dialog.exec()) return;

    lockGUI(true);
    QString errmsg;
    QFile file(dialog.selectedFiles().at(0));
    if (file.open(QFile::Truncate | QFile::WriteOnly)) {
        const QByteArray &text = ui->textSysInfo->toPlainText().toUtf8();
        if (file.write(text) != text.size()) errmsg = file.errorString();
        file.close();
    }

    lockGUI(false);
    showSavedMessage(file.fileName(), errmsg);
}
void MainWindow::on_pushSave_clicked()
{
    QFileDialog dialog(this, tr("Save System Information"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    const QStringList filters({"application/zip", "application/gzip", "application/octet-stream"});
    dialog.setMimeTypeFilters(filters); // Segmentation fault when using init list directory.
    dialog.setDefaultSuffix("zip");
    dialog.setDirectory(getenv("HOME")); // Must be done before selectFile()
    dialog.selectFile("sysinfo.zip");
    if (!dialog.exec()) return;

    lockGUI(true);
    struct archive *arc = nullptr;
    struct archive_entry *arcentry = nullptr;
    QString errmsg;
    const QByteArray &selfile = dialog.selectedFiles().at(0).toUtf8();
    try {
        arc = archive_write_new();
        arcentry = archive_entry_new();
        if (!arc || !arcentry) throw true;
        if (archive_write_set_format_filter_by_ext(arc, selfile.constData()) != ARCHIVE_OK) throw false;
        if (archive_write_open_filename(arc, selfile.constData()) != ARCHIVE_OK) throw false;

        for (int row = 0; row < ui->listInfo->count(); ++row) {
            const QListWidgetItem *item = ui->listInfo->item(row);
            assert(item != nullptr);
            if (item->checkState() != Qt::Checked) continue;

            QByteArray contents;
            switch(row) {
                case 0: contents = systeminfo().toUtf8(); break;
                case 1: contents = apthistory().toUtf8(); break;
                default: contents = readfile("/var/log/" + item->text()).toUtf8(); break;
            }
            archive_entry_set_pathname(arcentry, item->data(Qt::UserRole).toByteArray().constData());
            archive_entry_set_filetype(arcentry, AE_IFREG);
            archive_entry_set_perm(arcentry, 0644);
            archive_entry_set_mtime(arcentry, time(NULL), 0);
            archive_entry_set_size(arcentry, contents.size());
            if (archive_write_header(arc, arcentry) != ARCHIVE_OK) throw false;

            if (archive_write_data(arc, contents.constData(), contents.size()) != contents.size()) throw false;
            archive_entry_clear(arcentry);
        }
        if (archive_write_close(arc) != ARCHIVE_OK) throw false;
    } catch(bool sys) {
        if (!arc || sys) errmsg = strerror(errno);
        else errmsg = archive_error_string(arc);
    } catch(const QString &msg) {
        errmsg = msg;
    }
    if (arcentry) archive_entry_free(arcentry);
    if (arc) archive_write_free(arc);

    lockGUI(false);
    showSavedMessage(selfile, errmsg);
}
void MainWindow::showSavedMessage(const QString &filename, const QString &errmsg)
{
    QMessageBox msgbox(this);
    QPushButton *open = nullptr;
    if (errmsg.isEmpty()) {
        msgbox.setIcon(QMessageBox::Information);
        msgbox.setText(tr("System information saved."));
        open = msgbox.addButton(tr("Open folder"), QMessageBox::AcceptRole);
        // The dialog X button needs a RejectRole button to work.
        msgbox.addButton(tr("OK"), QMessageBox::RejectRole);
        open->setIcon(QIcon::fromTheme("folder"));
        msgbox.setInformativeText(filename);
    } else {
        msgbox.setIcon(QMessageBox::Critical);
        msgbox.setText(tr("Could not save system information."));
        msgbox.setInformativeText(errmsg);
    }
    msgbox.exec();
    if (open && msgbox.clickedButton() == open) {
        // Send the dbus message that opens the configured file manager with the file selected.
        runCmd("dbus-send --session --dest=org.freedesktop.FileManager1"
            " --type=method_call /org/freedesktop/FileManager1 org.freedesktop.FileManager1.ShowItems"
            " array:string:\"file://" + filename + "\" string:\"\"");
    }
}

QString MainWindow::systeminfo()
{
    QString snapshot = readfile("/etc/snapshot_created");
    if (!snapshot.isEmpty()) {
        snapshot.prepend("Snapshot created on: ");
        snapshot.append('\n');
    }
    Result out = runCmd(QStringLiteral("inxi -Fxxxrza -c0"));

    // Filtering
    const QString unamev = runCmd("uname -v | grep -oP '.*[[:space:]]\\K([0-9]+[.])+[^[:space:]]*'").output;
    static const QRegularExpression kernel_add("(.+Kernel:(" "\\x1b\\[[0-9;]+[mK]" "|[[:space:]])+[[:alnum:].-]+)(.*)");
    out.output.replace(kernel_add, "\\1 [" + unamev + "]\\3");
    static const QRegularExpression host_filter("(.+Host:(" "\\x1b\\[[0-9;]+[mK]" "|[[:space:]])+)([[:alnum:].-]+)(.*)");
    out.output.replace(host_filter, "\\1<filter>\\4");
    static const QRegularExpression uuid_filter("[[:xdigit:]]{8}-([[:xdigit:]]{4}-){3}[[:xdigit:]]{12}");
    out.output.replace(uuid_filter, "<filter>");
    // Final filtering
    out.output.replace("http: /", "http:/");
    out.output.replace("https: /", "https:/");

    // Extra information not provided by inxi
    out.output.append("\n\nBoot Mode: ");
    if (QFileInfo("/sys/firmware/efi").isDir()) out.output.append("UEFI");
    else out.output.append("BIOS (legacy, CSM, MBR)");
    Result sb = runCmd("(mokutil --sb-state || bootctl --no-variables status)"
        " 2>/dev/null | sed -nr 's/^\\s*Secure\\s?Boot:?/SecureBoot/p'");
    if (sb.output.contains("enabled")) out.output.append('\n' + sb.output);
    const QString &video_tweaks = readfile("/live/config/video-tweaks", false);
    if (!video_tweaks.isEmpty()) {
        out.output.append("\nVideo Tweaks:\n" + video_tweaks);
    }

    return snapshot + out.output.trimmed();
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
        item->setData(Qt::UserRole, logfile);
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
    item->setData(Qt::UserRole, "inxi.txt");
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
    autoFitSplitter();
}

void MainWindow::on_ButtonHelp_clicked()
{
    const QString &url = QStringLiteral("file:///usr/share/doc/quick-system-info-gui/quick-system-info-gui.html");
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

QString MainWindow::readfile(const QString &logfile, bool escalate)
{
    QString text;
    QFileInfo qfi(logfile);
    if (!qfi.isFile()) return text; // Treat non-existent files as empty.
    if (qfi.permission(QFile::ReadOther)){
        QFile file(logfile);
        if (!file.open(QIODevice::ReadOnly)) throw file.errorString();

        QTextStream in(&file);
        while(!in.atEnd()) {
            text += in.readAll();
        }
        file.close();
    } else if (escalate) {
        text = runCmd("pkexec /usr/lib/quick-system-info-gui/qsig-lib readadminfile /var/log/" + logfile).output;
    }

    return text.trimmed();
}

// The currentRowchanged() signal occurs before the selection change is displayed.
void MainWindow::on_listInfo_itemSelectionChanged()
{
    lockGUI(true);

    try {
        const int selrow = ui->listInfo->currentRow();
        switch(selrow){
        case 0:
            ui->textSysInfo->setPlainText(systeminfo());
            break;

        case 1:
            ui->textSysInfo->setPlainText(apthistory());
            break;

        default:
            ui->textSysInfo->setPlainText(readfile("/var/log/" + ui->listInfo->item(selrow)->text()));
            break;
        }
    } catch (const QString &msg) {
        QMessageBox::critical(this, windowTitle(), msg);
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
    const QString ctltext = tr("&Save (×%1)...").arg(nchecked);
    ui->pushSave->setEnabled(nchecked > 0);
    ui->pushSave->setText(ctltext);
    assert(actionSave != nullptr);
    actionSave->setEnabled(nchecked > 0);
    actionSave->setText(ctltext);
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
    if (lcount > 0) {
        // Quick System Info should always be selected.
        ui->listInfo->item(0)->setCheckState(Qt::Checked);
    }
    for(int row = 1; row < lcount; ++row) {
        QListWidgetItem *item = ui->listInfo->item(row);
        assert(item != nullptr);
        const bool sel = defaultMatches.contains(item->data(Qt::UserRole).toString(),
            Qt::CaseInsensitive);
        item->setCheckState(sel ? Qt::Checked : Qt::Unchecked);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == ui->splitter->handle(1)) autoFitSplitter();
    }
    return false;
}
// Auto-resize splitter to fit list column.
void MainWindow::autoFitSplitter()
{
    QList<int> sizes = ui->splitter->sizes();
    if(sizes.count() > 1) {
        int shint = ui->listInfo->sizeHintForColumn(0);
        if (sizes.at(0) <= 0) {
            // If the list is collapsed, pre-size it to ensure correct calculations.
            sizes[0] = shint;
            sizes[1] -= shint;
            ui->splitter->setSizes(sizes);
            sizes = ui->splitter->sizes();
        }
        const int total = sizes.at(0) + sizes.at(1);
        shint += sizes.at(0) - ui->listInfo->viewport()->contentsRect().width();
        sizes[0] = shint;
        sizes[1] = total - shint;
        ui->splitter->setSizes(sizes);
    }
}