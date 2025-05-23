/**********************************************************************
 *  mainwindow.cpp
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Dolphin Oracle, AK-47, Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of Quick System Info.
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
#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QRadioButton>
#include <QScreen>
#include <QTimer>
#include <QClipboard>
#include <archive.h>
#include <archive_entry.h>
#include <QByteArray>

#include "about.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent) noexcept
    : QDialog(parent)
    , ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    lockGUI(true);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    connect(ui->pushCancel, &QPushButton::clicked, this, &MainWindow::close);
    ui->textSysInfo->setWordWrapMode(QTextOption::NoWrap);
    ui->textSysInfo->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->textSysInfo->setPlainText(tr("Loading..."));
    resize(QGuiApplication::primaryScreen()->availableGeometry().size() * 0.6);
    ui->listInfo->setContextMenuPolicy(Qt::ActionsContextMenu);
    defaultMatches = arg_parser.positionalArguments();
    //don't show journald tab if not running systemd
    systemd_check();
    // This fires the lengthy setup routine after the window is displayed.
    QTimer::singleShot(0, this, &MainWindow::setup);
}

MainWindow::~MainWindow() { delete ui; }

// setup versious items first time program runs
void MainWindow::setup() noexcept
{
    // Allow user-friendly match strings.
    for(QString &match : defaultMatches) {
        if (!match.contains('.')) match.append(".txt");
    }

    // Basic control slots
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::tabWidget_currentChanged);
    connect(ui->listInfo, &QListWidget::itemSelectionChanged, this, &MainWindow::listInfo_itemSelectionChanged);
    connect(ui->listInfo, &QListWidget::itemChanged, this, &MainWindow::listInfo_itemChanged);
    connect(ui->pushSave, &QPushButton::clicked, this, &MainWindow::pushSave_clicked);
    connect(ui->pushSaveText, &QPushButton::clicked, this, &MainWindow::pushSaveText_clicked);
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushHelp, &QPushButton::clicked, this, &MainWindow::pushHelp_clicked);

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
    connect(saveasfile, &QAction::triggered, this, &MainWindow::pushSaveText_clicked);
    QAction *find = new QAction(QIcon::fromTheme("search"), tr("&Find..."), this);
    find->setShortcutVisibleInContextMenu(true);
    find->setShortcut(Qt::CTRL | Qt::Key_F);
    connect(find, &QAction::triggered, this, &MainWindow::showFindDialog);
    QAction *findnext = new QAction(QIcon::fromTheme("search"), tr("Find &Next"), this);
    findnext->setShortcutVisibleInContextMenu(true);
    findnext->setShortcut(Qt::Key_F3);
    connect(findnext, &QAction::triggered, this, &MainWindow::findNext);

    ui->textSysInfo->addAction(forumcopyaction);
    ui->textSysInfo->addAction(plaincopyaction);
    ui->textSysInfo->addAction(saveasfile);
    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    ui->textSysInfo->addAction(sep);
    ui->textSysInfo->addAction(find);
    ui->textSysInfo->addAction(findnext);

    //add actions to journald if set up
    if (journald_setup_done) {
        ui->plainTextEditJournald->addAction(forumcopyaction);
        ui->plainTextEditJournald->addAction(plaincopyaction);
        ui->plainTextEditJournald->addAction(saveasfile);
        ui->plainTextEditJournald->addAction(sep);
        ui->plainTextEditJournald->addAction(find);
        ui->plainTextEditJournald->addAction(findnext);
    }

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
    connect(actionSave, &QAction::triggered, this, &MainWindow::pushSave_clicked);

    ui->listInfo->addAction(selall);
    ui->listInfo->addAction(seldef);
    ui->listInfo->addAction(actionSave);

    ui->splitter->handle(1)->installEventFilter(this);
    buildInfoList();
    ui->listInfo->setCurrentRow(0);

    connect(ui->pushForumCopy, &QPushButton::clicked, this, &MainWindow::forumcopy);
    ui->pushForumCopy->setDefault(true);
    lockGUI(false);
}

void MainWindow::lockGUI(bool lock) noexcept
{
    if (ui->listInfo->isEnabled() == lock) {
        ui->listInfo->setDisabled(lock);
        ui->textSysInfo->setDisabled(lock);
        ui->pushSave->setDisabled(lock);
        ui->pushSaveText->setDisabled(lock);
        ui->pushForumCopy->setDisabled(lock);
        ui->tabWidget->setDisabled(lock);
        if (lock) QApplication::setOverrideCursor(Qt::WaitCursor);
        else {
            ui->listInfo->setFocus();
            QApplication::restoreOverrideCursor();
        }
    }
}

// Util function for getting bash command output and error code
int MainWindow::run(const char *program, const QStringList &args,
    QByteArray *output, const QByteArray *input) noexcept
{
    QEventLoop loop;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proc, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    proc.start(program, args);
    if (input) proc.write(*input);
    proc.closeWriteChannel();
    loop.exec();

    const bool ok = (proc.exitStatus()==QProcess::NormalExit && proc.error()==QProcess::UnknownError);
    if (output) {
        *output = proc.readAll().trimmed();
        if (!ok && output->isEmpty()) *output = proc.errorString().toUtf8();
    }
    return ok ? proc.exitCode() : -127;
}

void MainWindow::pushSaveText_clicked() noexcept
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
    if (ui->tabWidget->currentIndex() == 0){
        if (file.open(QFile::Truncate | QFile::WriteOnly)) {
            const QByteArray &text = ui->textSysInfo->toPlainText().toUtf8();
            if (file.write(text) != text.size()) errmsg = file.errorString();
            file.close();
        }
    } else {
        if (file.open(QFile::Truncate | QFile::WriteOnly)) {
            const QByteArray &text = ui->plainTextEditJournald->toPlainText().toUtf8();
            if (file.write(text) != text.size()) errmsg = file.errorString();
            file.close();
        }
    }
    lockGUI(false);
    showSavedMessage(file.fileName(), errmsg);
}
void MainWindow::pushSave_clicked() noexcept
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

            const QByteArray &contents = readReport(row);
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
void MainWindow::showSavedMessage(const QString &filename, const QString &errmsg) noexcept
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
        run("dbus-send", {"--session", "--dest=org.freedesktop.FileManager1", "--type=method_call",
            "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1.ShowItems",
            "array:string:file://" + filename, "string:"});
    }
}

void MainWindow::buildInfoList() noexcept
{
    QByteArray loglistout;
    const int logrc = run("pkexec", {"/usr/lib/quick-system-info-gui/qsig-lib-list", "list"}, &loglistout);

    ui->listInfo->blockSignals(true);
    ui->listInfo->clear();
    if (logrc == 0) {
        QStringList logfiles = QString(loglistout).split("\n");
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
    }

    // Special treatment for QSI because of how important it is.
    QListWidgetItem *item = new QListWidgetItem(tr("Quick System Info"));
    QFont ifont = item->font();
    ifont.setBold(true);
    item->setFont(ifont);
    item->setData(Qt::UserRole, "quick-system-info.txt");
    ui->listInfo->insertItem(0, item);
    // Special apt history info
    item = new QListWidgetItem("apt " + tr("history"));
    item->setData(Qt::UserRole, "apthistory.txt");
    if (QFile::exists("/var/log/dpkg.log")){
        ui->listInfo->insertItem(1, item);
    }
    listSelectDefault();
    ui->listInfo->blockSignals(false);
    listInfo_itemChanged(); // Set up multi buttons.
    // Resize the splitter according to the new contents
    QApplication::processEvents(); // Allow the scroll bar to materialise
    ui->tabWidget->setCurrentIndex(0);
    autoFitSplitter();
    // Stop the left pane from resizing with the window.
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);

    // Warn on failure to obtain the log file list earlier.
    if (logrc != 0) QMessageBox::warning(this, windowTitle(), loglistout);
}

void MainWindow::forumcopy() noexcept
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text;
    if (ui->tabWidget->currentIndex() == 0){
        text = ui->textSysInfo->textCursor().selectedText();
        if (text.isEmpty()) {
            text = ui->textSysInfo->toPlainText();
        }
    } else {
        text = ui->plainTextEditJournald->textCursor().selectedText();
        if (text.isEmpty()) {
            text = ui->plainTextEditJournald->toPlainText();
        }
    }
    text.replace(QChar(0x2029), "\n");
    clipboard->setText("[CODE]" + text + "[/CODE]");
}

void MainWindow::plaincopy() noexcept
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = ui->textSysInfo->textCursor().selectedText();
    text.replace(QChar(0x2029), "\n");
    if (text.isEmpty()) {
        text = ui->textSysInfo->toPlainText();
    }
    clipboard->setText(text);
}

QByteArray MainWindow::readReport(int row)
{
    QByteArray output;
    int execrc = 0;
    switch(row) {
        case 0: { // Quick System Info
            execrc = run("/usr/bin/quick-system-info-mx", {"-g"}, &output);
            // Deal with bugs in inxi.
            output.replace("http: /", "http:/");
            output.replace("https: /", "https:/");
            break;
        }
        case 1: { // apt history
            execrc = shell(QStringLiteral("zgrep -EH ' install | upgrade | purge | remove ' /var/log/dpkg*"
                " | cut -f2- -d: | sort -r | sed 's/ remove / remove  /;s/ purge / purge   /'"
                " | grep \"^\""), &output);
            break;
        }
        default: { // Other log files
            QListWidgetItem *item = ui->listInfo->item(row);
            assert (item != nullptr);
            QFileInfo qfi("/var/log/" + item->data(Qt::UserRole).toString());
            if (qfi.permission(QFile::ReadOther)){
                QFile file(qfi.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly)) throw file.errorString();
                output = file.readAll().trimmed();
                file.close();
            } else {
                execrc = run("pkexec", {"/usr/lib/quick-system-info-gui/qsig-lib",
                    "readadminfile", qfi.absoluteFilePath()}, &output);
            }
            break;
        }
    }
    static const QRegularExpression ansiEscapeRegex("\x1B\\[[0-9;]*[mG]");

    if (execrc != 0) throw (output);
    return QString::fromUtf8(output).remove(ansiEscapeRegex).toUtf8();

}

// The currentRowchanged() signal occurs before the selection change is displayed.
void MainWindow::listInfo_itemSelectionChanged() noexcept
{
    lockGUI(true);

    try {
        ui->textSysInfo->setPlainText(readReport(ui->listInfo->currentRow()));
    } catch (const QString &msg) {
        QMessageBox::critical(this, windowTitle(), msg);
        ui->textSysInfo->clear();
    }

    lockGUI(false);
}

void MainWindow::listInfo_itemChanged() noexcept
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
void MainWindow::listSelectAll() noexcept
{
    for(int row = 0; row < ui->listInfo->count(); ++row) {
        ui->listInfo->item(row)->setCheckState(Qt::Checked);
    }
}
void MainWindow::listSelectDefault() noexcept
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

void MainWindow::showFindDialog() noexcept
{
    QDialog dialog(ui->textSysInfo);
    if (ui->tabWidget->currentIndex() == 1 ) QDialog dialog(ui->plainTextEditJournald);
    dialog.setWindowTitle(tr("Find"));

    // Search text
    QLabel *labelFindWhat = new QLabel(tr("&Find what:"), &dialog);
    QLineEdit *editFind = new QLineEdit(&dialog);
    labelFindWhat->setBuddy(editFind);
    // Find Next and Cancel buttons
    QPushButton *pushFindNext = new QPushButton(tr("Find &Next"), &dialog);
    QPushButton *pushCancel = new QPushButton(tr("Cancel"), &dialog);
    pushFindNext->setDisabled(true); // Disable until there is text
    pushFindNext->setDefault(true);

    // Case sensitivity and whole words
    QCheckBox *checkCase = new QCheckBox(tr("Match c&ase"), &dialog);
    QCheckBox *checkWhole = new QCheckBox(tr("Match &whole word only"), &dialog);
    checkCase->setChecked(searchFlags.testFlag(QTextDocument::FindCaseSensitively));
    // Direction
    QGroupBox *groupDirection = new QGroupBox(tr("Direction"), &dialog);
    QHBoxLayout layoutDirection(groupDirection);
    QRadioButton *radioUp = new QRadioButton(tr("&Up"), &dialog);
    QRadioButton *radioDown = new QRadioButton(tr("&Down"), &dialog);
    const bool up = searchFlags.testFlag(QTextDocument::FindBackward);
    radioUp->setChecked(up);
    radioDown->setChecked(!up);
    layoutDirection.addWidget(radioUp);
    layoutDirection.addWidget(radioDown);

    QGridLayout layout(&dialog);
    layout.setSizeConstraint(QLayout::SetFixedSize);
    layout.addWidget(labelFindWhat, 0, 0);
    layout.addWidget(editFind, 0, 1, 1, 2);
    layout.addWidget(pushFindNext, 0, 3);
    layout.addWidget(pushCancel, 1, 3);
    layout.addWidget(checkCase, 1, 0, 1, 2);
    layout.addWidget(checkWhole, 2, 0, 1, 2);
    layout.addWidget(groupDirection, 1, 2, 2, 1);

    connect(pushCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(editFind, &QLineEdit::textChanged, &dialog, [=](const QString &text) {
        pushFindNext->setDisabled(text.isEmpty());
    });
    editFind->setText(searchText);
    editFind->selectAll();
    // The user should be able to interact with the window while searching.
    connect(pushFindNext, &QPushButton::clicked, &dialog, [=]() {
        searchText = editFind->text();
        searchFlags.setFlag(QTextDocument::FindCaseSensitively, checkCase->isChecked());
        searchFlags.setFlag(QTextDocument::FindWholeWords, checkWhole->isChecked());
        searchFlags.setFlag(QTextDocument::FindBackward, radioUp->isChecked());
        if (!searchText.isEmpty()) findNext();
    });

    // Start the dialog
    QEventLoop loop;
    connect(&dialog, &QDialog::rejected, &loop, &QEventLoop::quit);
    dialog.show();
    loop.exec();
}
void MainWindow::findNext() noexcept
{
    if (searchText.isEmpty()) {
        showFindDialog();
    } else {
        if (ui->tabWidget->currentIndex() == 0){
            if (!ui->textSysInfo->find(searchText, searchFlags)) {
                QMessageBox::information(this, windowTitle(), tr("Cannot find \"%1\"").arg(searchText));
            }
        }
        if (ui->tabWidget->currentIndex() == 1){
            if (!ui->plainTextEditJournald->find(searchText, searchFlags)) {
                QMessageBox::information(this, windowTitle(), tr("Cannot find \"%1\"").arg(searchText));
            }
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) noexcept
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == ui->splitter->handle(1)) autoFitSplitter();
    }
    return false;
}
// Auto-resize splitter to fit list column.
void MainWindow::autoFitSplitter() noexcept
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

void MainWindow::systemd_check()
{   QByteArray output;
    int test = run("ps",{"-p","1","-o","comm="},&output);
    QString systemd = "systemd";
    if (test == 0){
        if (QString(output) != systemd){
            ui->tabWidget->removeTab(1);
        }
    }
}

void MainWindow::journald_setup()
{   QByteArray output;
    int test = 0;
    //set some word wrap modes
    ui->plainTextEditJournald->setWordWrapMode(QTextOption::NoWrap);
    ui->plainTextEditJournald->setContextMenuPolicy(Qt::ActionsContextMenu);

    //set default options
    //priority levels 1-7.  4 is somewhat usefull for user level, 3 usually empty
    ui->comboBoxJournaldPriority->setCurrentIndex(4);

    //index 1 is user level, which requires no root permissions.  index 0 is system (root) level
    //set system if /run/log/journal/$(machine-id) exists, as that indicates either /var/log/journal is missing or volatile storage in use
    //either way user permissions won't be suffiencent to list boots properly, so elevate and and remove user item option.

    //harvest machine-id
    QString machine_id;
    test = run("cat",{"/etc/machine-id"},&output);
    if (test == 0){
        machine_id = output;
    }
    qDebug() << "machine ID is" << machine_id;

    if (QDir("/run/log/journal/" + machine_id).exists()) {
        ui->comboBoxJournaldSystemUser->setCurrentIndex(0);
        ui->comboBoxJournaldSystemUser->removeItem(1);
        test = run("pkexec",{"/usr/lib/quick-system-info-gui/qsig-lib","journalctl_command","journalctl","--list-boots","--no-pager","-q","-r"},&output);
    } else {
        test = run("journalctl",{"--list-boots","--no-pager","-q","-r"},&output);
        ui->comboBoxJournaldSystemUser->setCurrentIndex(1);

    }

    if (test == 0){
        const QStringList bootlist = QString(output).split("\n");
        //bootlist.sort();
        //trim strings because journalctl output has leading spaces
        for(const QString &item : bootlist) {
            ui->comboBoxJournaldListBoots->addItem(item.trimmed());
        }
    }
    //flag that setup has been done, now changes in combo boxes will instantly change report
    journald_setup_done = true;
}

void MainWindow::run_journalctl_report()
{
    if (!journald_setup_done) {
        return;
    }

    QByteArray output;
    QString text;

    //searchoption is for the --unit=UNIT parameter, which works to filter by service
    QString searchoption;
    if (!ui->lineEditJournaldSearch->text().isEmpty()) {
        searchoption="--unit=" + ui->lineEditJournaldSearch->text();
    }
    //bootoption works off UUID of selection in boot list combbox
    QString bootoption = ui->comboBoxJournaldListBoots->currentText().section(" ",1,1);
    if (ui->comboBoxJournaldListBoots->currentIndex() == 0){
        bootoption = "--boot";
    } else {
        bootoption.prepend("--boot=");
    }

    //user or system, using indexes so translations can work
    QString adminlevel;
    if (ui->comboBoxJournaldSystemUser->currentIndex() == 1) {
        adminlevel = "--user";
    } else {
        adminlevel = "--system";
    }
    //log level.  higher the number of priority level, the more information presented
    QString priority = ui->comboBoxJournaldPriority->currentText().prepend("--priority=");

    qDebug() << "bootoption is " << bootoption;
    qDebug() << "adminLevel is " << adminlevel;
    qDebug() << "priority is " << priority;

    //no pkexec needed if user level, otherwise use pkexec to run a report with elevated rights
    int test=0;
    if (adminlevel == "--user"){
        if (searchoption.isEmpty()){
            test = run("journalctl",{bootoption,adminlevel,priority,"--no-pager","-q"},&output);
        } else {
            test = run("journalctl",{bootoption,adminlevel,priority,"--no-pager","-q",searchoption,"--case-sensitive=false"},&output);
        }
    } else {
        if (searchoption.isEmpty()){
            test = run("pkexec",{"/usr/lib/quick-system-info-gui/qsig-lib","journalctl_command","journalctl",bootoption,adminlevel,priority,"--no-pager","-q"},&output);
        } else {
            test = run("pkexec",{"/usr/lib/quick-system-info-gui/qsig-lib","journalctl_command","journalctl",bootoption,adminlevel,priority,"--no-pager","-q",searchoption,"--case-sensitive=false"},&output);
        }
    }
    //qDebug() << "output is " << QString(output);
    //qDebug() << "test is " << test;

    //if command succesful, but not output to display, say so
    //show error if non 0 exit
    if (test == 0){
        text = QString(output);
        if (text.isEmpty()) text = tr("No journal entries found at this admin and priority level","no journal entries found at the options specified");
    } else {
        text = tr("Error running journalctl command","error report for journalctl command");
    }
    ui->plainTextEditJournald->setPlainText(text);
}

//actions when report is run
//run journalctl reports when tab changes to journald tab
//or when any option combo box is edited
//or when search reload button is used
//run journald setup if hasn't happened yet.

void MainWindow::tabWidget_currentChanged(int index) noexcept
{
    if (index == 1) {
        if (!journald_setup_done) {
            journald_setup();

            QAction *actionJournaldSearch = new QAction(QIcon::fromTheme(QStringLiteral("search")),
                                                        tr("Search for this service"), ui->lineEditJournaldSearch);
            ui->lineEditJournaldSearch->addAction(actionJournaldSearch, QLineEdit::TrailingPosition);
            actionJournaldSearch->setShortcut(Qt::Key_Return);
            actionJournaldSearch->setShortcutContext(Qt::WidgetShortcut);
            connect(ui->comboBoxJournaldListBoots, &QComboBox::activated, this, &MainWindow::run_journalctl_report);
            connect(ui->comboBoxJournaldPriority, &QComboBox::activated, this, &MainWindow::run_journalctl_report);
            connect(ui->comboBoxJournaldSystemUser, &QComboBox::activated, this, &MainWindow::run_journalctl_report);
            connect(actionJournaldSearch, &QAction::triggered, this, &MainWindow::run_journalctl_report);
        }
        //hide save files button as it has no place on this tab
        ui->pushSave->hide();
        run_journalctl_report();
    } else {
        //show pushSave
        ui->pushSave->show();
    }
}

// About button clicked
void MainWindow::pushAbout_clicked() noexcept
{
    displayAboutMsgBox(
        tr("About Quick-System-Info-gui"),
        "<p align=\"center\"><b><h2>" + tr("Quick System Info") + "</h2></b></p><p align=\"center\">" + tr("Version: ")
            + VERSION + "</p><p align=\"center\"><h3>" + tr("Program for displaying a quick system info report")
            + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
              "<p align=\"center\">"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        QStringLiteral("/usr/share/doc/quick-system-info-gui/license.html"),
        tr("%1 License").arg(this->windowTitle()));
}

void MainWindow::pushHelp_clicked() noexcept
{
    const QString &url = QStringLiteral("file:///usr/share/doc/quick-system-info-gui/quick-system-info-gui.html");
    displayDoc(url, tr("%1 Help").arg(tr("Quick System Info")));
}
