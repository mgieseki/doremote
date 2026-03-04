// Copyright (C) 2026 Martin Gieseking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QLineEdit>
#include <QMessageBox>
#include "MainWindow.hpp"
#include "./ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settings_("MG", "DoremoteTest")
{
    ui->setupUi(this);
    connect(ui->commandEdit->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::on_commandEdit_returnPressed);
    statusBar()->showMessage("Not connected to Dorico.");
    doremote_ = doremote_create_instance();
}

MainWindow::~MainWindow() {
    doremote_destroy_instance(doremote_);
    delete ui;
}

void MainWindow::on_actionConnect_button_toggled (bool activate) {
    if (activate) {
        ui->actionConnect_button->setToolTip("Disconnect from Dorico");
        doricoConnect();
    }
    else {
        ui->actionConnect_button->setToolTip("Connect to Dorico");
        doricoDisconnect();
    }
}

void MainWindow::on_actionUpdateStatus_button_triggered() {
    doremote_update_status(doremote_);
}

void MainWindow::on_commandEdit_returnPressed() {
    QString command = ui->commandEdit->text();
    doremote_send_command(doremote_, command.toUtf8().constData());
    char response[1024];
    ui->textOutputArea->setPlainText(doremote_get_response(doremote_, response, 1024));
}

void MainWindow::doricoConnect () {
    if (doremote_session_token(doremote_))
        return;
    int status = DOREMOTE_OFFLINE;
    auto token = settings_.value("token", "").toString();
    if (!token.isEmpty()) {
        // try to connect using the previous session token
        status = doremote_reconnect(doremote_, "Doremote Qt Test Client", "127.0.0.1", "4560", token.toUtf8().constData());
    }
    bool reconnected = (status == DOREMOTE_CONNECTED);
    if (!reconnected) {
        // try to establish a new connection (requires confirmation in Dorico)
        statusBar()->showMessage("Waiting for connection. Please confirm the request in Dorico.");
        qApp->processEvents();
        status = doremote_connect(doremote_, "Doremote Qt Test Client", "127.0.0.1", "4560");
    }
    switch (status) {
        case DOREMOTE_CONNECTED: {
            if (!reconnected) {
                // save session token for future connections
                token = doremote_session_token(doremote_);
                settings_.setValue("token", token);
            }
            DoricoAppInfo info;
            doremote_get_app_info(doremote_, &info);
            statusBar()->showMessage(
                "Connected to Dorico " + QString(info.variant) + " " + QString(info.number)
                + " (session token: " + token + ")");
            ui->actionUpdateStatus_button->setEnabled(true);
            doremote_set_status_callback(doremote_, &MainWindow::status_updated, this);
            statusUpdater_.assign(ui->statusTable);
            populateCommandBox();
            break;
        }
        case DOREMOTE_OFFLINE:
        case DOREMOTE_CONNECTION_DENIED:
            ui->actionConnect_button->setChecked(false);
            statusBar()->showMessage("Not connected to Dorico.");
            QMessageBox::critical(this, "Connection Error",
                status == DOREMOTE_OFFLINE
                ? "Can't connect to Dorico. It's probably not running."
                : "Connection to Dorico denied.");
            break;
    }
}

void MainWindow::doricoDisconnect() {
    doremote_disconnect(doremote_);
    ui->statusTable->setRowCount(0);
    ui->commandEdit->clearItems();
    ui->textOutputArea->clear();
    ui->actionUpdateStatus_button->setEnabled(false);
    statusBar()->showMessage("Not connected to Dorico.");
}

void MainWindow::status_updated (KeyValuePair *status, int size, void *userdata) {
    auto self = static_cast<MainWindow*>(userdata);
    if (!status || !self)
        return;
    self->statusUpdater_.enqueue(status, size);
}

static QString format_tooltip (const QString &cmd, const QString &param_str) {
    QString ret = "<h2>"+cmd+"</h2>";
    if (param_str.isEmpty())
        ret += "no parameters expected";
    else {
        QStringList params = param_str.split(",");
        auto idx_optional = params.indexOf(QRegularExpression(R"(^\[.+\]$)"));
        if (idx_optional < 0)
            idx_optional = params.size();
        if (idx_optional > 0) {
            ret += "<h3>Mandatory Parameters</h3><ul>";
            for (int i=0; i < idx_optional; i++)
                ret += "<li>" + params[i] + "<li/>";
            ret += "</ul>";
        }
        if (idx_optional < params.size()) {
            ret += "<h3>Optional Parameters</h3><ul>";
            for (int i=idx_optional; i < params.size(); i++)
                ret += "<li>" + params[i].mid(1, params[i].size()-2) + "</li>";
            ret += "</ul>";
        }
    }
    return ret;
}

void MainWindow::populateCommandBox () {
    if (int count = doremote_get_commands(doremote_, nullptr, nullptr)) {
        std::vector<KeyValuePair> commands(count);
        if (doremote_get_commands(doremote_, &commands[0], &count)) {
            QList<QPair<QString,QString>> item_pairs;
            for (auto &[cmd, params] : commands)
                item_pairs.append(QPair<QString,QString>(cmd, format_tooltip(cmd, params)));
            ui->commandEdit->setItemPairs(item_pairs);
        }
    }
}
