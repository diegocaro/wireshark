/** @file
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TAP_PARAMETER_DIALOG_H
#define TAP_PARAMETER_DIALOG_H

/*
 * @file Base class for statistics and analysis dialogs.
 * Provides convenience classes for command-line tap parameters ("-z ...")
 * and general tapping.
 */

#include "config.h"

#include <epan/stat_groups.h>
#include <epan/stat_tap_ui.h>

#include <QMenu>

#include "filter_action.h"
#include "wireshark_dialog.h"

class QHBoxLayout;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace Ui {
class TapParameterDialog;
}

class TapParameterDialog;
typedef TapParameterDialog* (*tpdCreator)(QWidget &parent, const QString cfg_str, const QString arg, CaptureFile &cf);

class TapParameterDialog : public WiresharkDialog
{
    Q_OBJECT

public:
    explicit TapParameterDialog(QWidget &parent, CaptureFile &cf, int help_topic = 0);
    ~TapParameterDialog();

    static const QString &actionName() { return action_name_; }
    static void registerDialog(const QString title, const char *cfg_abbr, register_stat_group_t group, stat_tap_init_cb tap_init_cb, tpdCreator creator);

    static TapParameterDialog *showTapParameterStatistics(QWidget &parent, CaptureFile &cf, const QString cfg_str, const QString arg, void *);
    // Needed by static member functions in subclasses. Should we just make
    // "ui" available instead?
    QTreeWidget *statsTreeWidget();
    QLineEdit *displayFilterLineEdit();
    QPushButton *applyFilterButton();
    QVBoxLayout *verticalLayout();
    QHBoxLayout *filterLayout();

    void drawTreeItems();

signals:
    void filterAction(QString filter, FilterAction::Action action, FilterAction::ActionType type);
    void updateFilter(QString filter);

public slots:

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void addFilterActions();
    void addTreeCollapseAllActions();
    QString displayFilter();
    void setDisplayFilter(const QString &filter);
    void setHint(const QString &hint);
    // Retap packets on first display. RPC stats need to disable this.
    void setRetapOnShow(bool retap);

protected slots:
    void filterActionTriggered();
    void collapseAllActionTriggered();
    void expandAllActionTriggered();
    void updateWidgets();

private:
    Ui::TapParameterDialog *ui;
    QMenu ctx_menu_;
    QList<QAction *> filter_actions_;
    int help_topic_;
    static const QString action_name_;
    QTimer *show_timer_;

    virtual const QString filterExpression() { return QString(); }
    QString itemDataToPlain(QVariant var, int width = 0);
    virtual QList<QVariant> treeItemData(QTreeWidgetItem *) const;
    virtual QByteArray getTreeAsString(st_format_type format);

private slots:
    // Called by the constructor. The subclass should tap packets here.
    virtual void fillTree() = 0;

    void on_applyFilterButton_clicked();
    void on_actionCopyToClipboard_triggered();
    void on_actionSaveAs_triggered();
    void on_buttonBox_helpRequested();
};

#endif // TAP_PARAMETER_DIALOG_H
