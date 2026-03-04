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

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

class FilterDropDownEdit : public QWidget {
    Q_OBJECT
    public:
        explicit FilterDropDownEdit (QWidget *parent = nullptr) : QWidget(parent) {
            edit_ = new QLineEdit(this);
            auto *layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(edit_);

            src_ = new QStandardItemModel(this);

            proxy_ = new QSortFilterProxyModel(this);
            proxy_->setSourceModel(src_);
            proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

            dropFrame_ = std::make_unique<QFrame>();
            dropFrame_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
            dropFrame_->setFrameShape(QFrame::Box);
            dropFrame_->setLineWidth(1);
            dropFrame_->hide();

            auto *frameLay = new QVBoxLayout(dropFrame_.get());
            frameLay->setContentsMargins(0,0,0,0);

            view_ = new QListView(dropFrame_.get());
            view_->setModel(proxy_);
            view_->setSelectionMode(QAbstractItemView::SingleSelection);
            frameLay->addWidget(view_);

            edit_->installEventFilter(this);
            view_->installEventFilter(this);
            qApp->installEventFilter(this);  // handle outside clicks

            connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
                const QString tt = current.data(Qt::ToolTipRole).toString();
                if (!dropFrame_->isVisible() || !current.isValid())
                    QToolTip::hideText();
                else if (tt.isEmpty())
                    QToolTip::hideText();
                else {
                    const QPoint gp = edit_->mapToGlobal(QPoint(edit_->width()+8, edit_->height()/2));
                    QToolTip::showText(gp, tt, edit_);  // edit_ als "Owner" ist hier sinnvoll
                }
            });

            connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget *now) {
                if (dropFrame_->isVisible() && !isInEditOrDrop(now))
                    hideDropDown();
            });

            connect(edit_, &QLineEdit::textChanged, this, [this](const QString &t) {
                if (t.isEmpty()) {
                    proxy_->setFilterRegularExpression(QRegularExpression());
                    hideDropDown();
                    return;
                }
                QRegularExpression re(QRegularExpression::escape(t), QRegularExpression::CaseInsensitiveOption);
                proxy_->setFilterRegularExpression(re);
                showDropDown();
                ensureCurrentRowValid(true);
            });

            connect(view_, &QListView::clicked, this, [this](const QModelIndex &idx) {
                commitIndex(idx);
            });
        }

        ~FilterDropDownEdit () override {
            qApp->removeEventFilter(this);
        }

        void setItemPairs (const QList<QPair<QString, QString>> &item_pairs) {
            src_->clear();
            for (const auto &[cmd,params] : item_pairs) {
                auto item = new QStandardItem(cmd);
                item->setData(params, Qt::ToolTipRole);
                src_->appendRow(item);
            }
        }

        void clearItems ()              {src_->clear();}
        QString text () const           {return edit_->text();}
        void setText (const QString &t) {edit_->setText(t);}
        QLineEdit* lineEdit () const    {return edit_;}

    signals:
        void activated (const QString &text);

    protected:
        bool eventFilter (QObject *obj, QEvent *ev) override {
            if (obj == qApp && ev->type() == QEvent::MouseButtonPress && dropFrame_->isVisible()) {
                auto *me = static_cast<QMouseEvent*>(ev);
                const QPoint gp = me->globalPosition().toPoint();
                QWidget *w = QApplication::widgetAt(gp);
                if (!isInEditOrDrop(w))
                    hideDropDown();
            }
            if ((obj == this || obj == window()) && (ev->type() == QEvent::Move || ev->type() == QEvent::Resize)) {
                if (dropFrame_->isVisible())
                    positionDropDown();
            }
            if (obj == edit_ && ev->type() == QEvent::KeyPress) {
                auto *ke = static_cast<QKeyEvent*>(ev);

                switch (ke->key()) {
                    case Qt::Key_Down:
                        if (!edit_->text().isEmpty()) {
                            showDropDown();
                            ensureCurrentRowValid(true);
                            moveCurrent(+1);
                        }
                        return true;

                    case Qt::Key_Up:
                        if (!edit_->text().isEmpty()) {
                            showDropDown();
                            ensureCurrentRowValid(true);
                            moveCurrent(-1);
                        }
                        return true;

                    case Qt::Key_Escape:
                        if (dropFrame_->isVisible()) {
                            hideDropDown();
                            return true;
                        }
                        break;

                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                        if (dropFrame_->isVisible()) {
                            QModelIndex idx = view_->currentIndex();
                            if (!idx.isValid() && proxy_->rowCount() > 0)
                                idx = proxy_->index(0,0);
                            if (idx.isValid())
                                commitIndex(idx);
                            else
                                hideDropDown();
                            return true;
                        }
                        break;

                    default:
                        break;
                }
            }
            if (obj == view_ && ev->type() == QEvent::KeyPress) {
                auto *ke = static_cast<QKeyEvent*>(ev);

                if (ke->key() == Qt::Key_Escape) {
                    hideDropDown();
                    edit_->setFocus();
                    return true;
                }
                if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                    const QModelIndex idx = view_->currentIndex();
                    if (idx.isValid())
                        commitIndex(idx);
                    else
                        hideDropDown();
                    return true;
                }
            }
            return QWidget::eventFilter(obj, ev);
        }

        void resizeEvent (QResizeEvent *e) override {
            QWidget::resizeEvent(e);
            if (dropFrame_->isVisible())
                positionDropDown();
        }

        void moveEvent (QMoveEvent *e) override {
            QWidget::moveEvent(e);
            if (dropFrame_->isVisible())
                positionDropDown();
        }

        void showEvent (QShowEvent *e) override {
            QWidget::showEvent(e);
            if (window() && !windowFilterInstalled_) {
                window()->installEventFilter(this);
                windowFilterInstalled_ = true;
            }
        }

        void hideDropDown() {
            dropFrame_->hide();
            QToolTip::hideText();
        }

    private:
        bool isInEditOrDrop (QWidget *w) const {
            return w && (w == edit_ || w == dropFrame_.get() || dropFrame_->isAncestorOf(w));
        }

        void showDropDown () {
            if (window() && !windowFilterInstalled_) {
                window()->installEventFilter(this);
                windowFilterInstalled_ = true;
            }
            positionDropDown();
            dropFrame_->show();
            dropFrame_->raise();
        }

        void hideDropDown () const {
            dropFrame_->hide();
        }

        void positionDropDown () const {
            const QPoint p = edit_->mapToGlobal(QPoint(0, edit_->height()));
            const int w = edit_->width();
            const int h = 200; // max. height
            dropFrame_->setGeometry(QRect(p, QSize(w, h)));
        }

        void ensureCurrentRowValid (bool selectFirstIfNone) const {
            if (proxy_->rowCount() <= 0)
                view_->setCurrentIndex(QModelIndex());
            else if (!view_->currentIndex().isValid() && selectFirstIfNone)
                view_->setCurrentIndex(proxy_->index(0,0));
        }

        void moveCurrent (int delta) const {
            if (proxy_->rowCount() <= 0)
                return;
            int row = std::max(view_->currentIndex().row(), 0);
            row = std::clamp(row+delta, 0, proxy_->rowCount()-1);

            const QModelIndex idx = proxy_->index(row, 0);
            view_->setCurrentIndex(idx);
            view_->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }

        void commitIndex (const QModelIndex &idx) {
            edit_->setText(idx.data().toString());
            hideDropDown();
            edit_->setFocus();
            emit activated(edit_->text());
        }

    private:
        std::unique_ptr<QFrame> dropFrame_;
        QLineEdit *edit_ = nullptr;
        QListView *view_ = nullptr;
        QStandardItemModel *src_ = nullptr;
        QSortFilterProxyModel *proxy_ = nullptr;
        bool windowFilterInstalled_ = false;
};