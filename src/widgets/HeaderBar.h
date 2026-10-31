/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QString>
#include <QWidget>

class NavigationBar;
class QLabel;
class WindowButton;
class HeaderBar : public QWidget
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);

    NavigationBar *navigationBar() const;
    void setMaximized(bool maximized);
    void setAccountName(const QString &username);

signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void accountRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void startWindowMove();

    NavigationBar *m_navigationBar = nullptr;
    QLabel *m_userNameLabel = nullptr;
    WindowButton *m_minimizeButton = nullptr;
    WindowButton *m_maximizeButton = nullptr;
    WindowButton *m_closeButton = nullptr;
};
