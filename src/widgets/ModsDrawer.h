/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "SampleData.h"

#include <QImage>
#include <QList>
#include <QWidget>

class AccentButton;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QPropertyAnimation;
class QResizeEvent;
class QScrollArea;
class QVBoxLayout;
class ToggleSwitch;

class ModsDrawer : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal revealProgress READ revealProgress WRITE setRevealProgress)

public:
    explicit ModsDrawer(const VersionInfo &version, QWidget *parent = nullptr);

    void open();
    void closeDrawer();

    qreal revealProgress() const;
    void setRevealProgress(qreal progress);

signals:
    void closed();
    void modsChanged();
    void notifyRequested(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void updatePanelGeometry();
    void updateEnabledCount();
    void applyFilter(const QString &query);
    void setModEnabled(int index, bool enabled);

    VersionInfo m_version;
    QWidget *m_panel = nullptr;
    QLineEdit *m_search = nullptr;
    QVBoxLayout *m_modsLayout = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_noResultsLabel = nullptr;
    QList<QWidget *> m_rowWidgets;
    QList<QString> m_rowSearchKeys;
    QList<ToggleSwitch *> m_switches;
    QPropertyAnimation *m_animation = nullptr;
    QImage m_backgroundImage;
    int m_backgroundOpacity = 35;
    qreal m_progress = 0.0;
    bool m_closing = false;
};
