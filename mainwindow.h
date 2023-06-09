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
#include <QProcess>

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

protected:
    QProcess *proc {};

public:
    explicit MainWindow(const QCommandLineParser &arg_parser, QWidget *parent = nullptr);
    ~MainWindow();

    Result runCmd(const QString &cmd);

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
    void lockGUI(bool lock);
    void forumcopy();
    void plaincopy();
    QString systeminfo();
    QString apthistory();
    QString readlog(const QString &logfile);
    void buildInfoList();
    void listSelectAll();
    void listSelectDefault();
    bool eventFilter(QObject *watched, QEvent *event);
    void autoFitSplitter();
};

#endif // MAINWINDOW_H
