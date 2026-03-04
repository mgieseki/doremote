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

#pragma once

#include <QMainWindow>
#include <QSettings>
#include <doremote.h>
#include "StatusTableUpdater.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

    protected:
        void doricoConnect();
        void doricoDisconnect();
        static void status_updated(KeyValuePair *kv, int size, void *self);

    private slots:
        void on_actionConnect_button_toggled(bool activate);
        void on_actionUpdateStatus_button_triggered();
        void on_commandEdit_returnPressed();
        void populateCommandBox();

    private:
        Ui::MainWindow *ui;
        doremote_handle doremote_ = DOREMOTE_NULL_HANDLE;
        QSettings settings_;
        StatusTableUpdater statusUpdater_;
};
