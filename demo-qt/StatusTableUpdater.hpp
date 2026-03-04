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

#include <QHeaderView>
#include <QMutex>
#include <QObject>
#include <QTableWidget>
#include <doremote.h>

// use some STL containers for now as they support move semantics (unlike QQueue for example)
#include <vector>
#include <queue>

class StatusTableUpdater : public QObject {
    Q_OBJECT
    using StatusVector = std::vector<KeyValuePair>;

    public:
        StatusTableUpdater (QObject *parent=nullptr)
            : QObject(parent) {}

        explicit StatusTableUpdater (QTableWidget *table, QObject *parent=nullptr)
            : QObject(parent), table_(table) {}

        void assign (QTableWidget *table) {
            table_ = table;
        }

        void enqueue (KeyValuePair *kv, int count) {
            {
                QMutexLocker lock(&mutex_);
                StatusVector status;
                status.reserve(count);
                std::move(kv, kv+count, std::back_inserter(status));
                statusQueue_.push(std::move(status));
            }
            QMetaObject::invokeMethod(this, &StatusTableUpdater::drainQueue, Qt::QueuedConnection);
        }

    public slots:
        void drainQueue () {
            for (;;) {
                StatusVector status;
                {
                    QMutexLocker lock(&mutex_);
                    if (statusQueue_.empty())
                        break;
                    status = std::move(statusQueue_.front());
                    statusQueue_.pop();
                }
                populateTable(status);
            }
        }

    protected:
        void populateTable (const StatusVector &status) {
            if (!table_)
                return;
            table_->setColumnCount(2);
            table_->setRowCount(status.size());
            for (int i=0; i < status.size(); i++) {
                auto item0 = new QTableWidgetItem(status[i].key);
                auto item1 = new QTableWidgetItem(status[i].value);
                table_->setItem(i, 0, item0);
                table_->setItem(i, 1, item1);
            }
            table_->resizeColumnsToContents();
            table_->horizontalHeader()->setStretchLastSection(true);
        }

    private:
        QMutex mutex_;
        std::queue<StatusVector> statusQueue_;
        QTableWidget *table_ = nullptr;
};