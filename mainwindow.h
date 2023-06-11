/**********************************************************************
 *  mainwindow.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCommandLineParser>
#include <QDialog>
#include <QAction>
#include <QEvent>
#include <QTextDocument>

namespace Ui
{
class MainWindow;
}

struct Result {
    int exitCode;
    QString output;
};

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &arg_parser, QWidget *parent = nullptr);
    ~MainWindow();

    Result run(const char *program, const QStringList &args = QStringList(),
        const QString *input = nullptr);
    inline Result shell(const QString &cmd, const QString *input = nullptr)
    {
        return run("/bin/bash", {"-c", cmd}, input);
    }

    void setup();

private slots:
    void on_pushSave_clicked();
    void on_pushSaveText_clicked();
    void on_buttonAbout_clicked();
    void on_ButtonHelp_clicked();
    void on_listInfo_itemSelectionChanged();
    void on_listInfo_itemChanged();

private:
    Ui::MainWindow *ui;
    QStringList defaultMatches;
    QAction *actionSave = nullptr;
    QString searchText;
    QTextDocument::FindFlags searchFlags;
    void lockGUI(bool lock);
    void forumcopy();
    void plaincopy();
    void showSavedMessage(const QString &filename, const QString &errmsg);
    QString systeminfo();
    QString apthistory();
    QString readfile(const QString &path, bool escalate = true);
    void buildInfoList();
    void listSelectAll();
    void listSelectDefault();
    void showFindDialog();
    void findNext();
    bool eventFilter(QObject *watched, QEvent *event);
    void autoFitSplitter();
};

#endif // MAINWINDOW_H
