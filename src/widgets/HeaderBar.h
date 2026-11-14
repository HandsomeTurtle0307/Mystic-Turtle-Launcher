/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QWidget>

class AvatarWidget;
class NavigationBar;
class QLabel;
class QPushButton;
class WindowButton;
class HeaderBar : public QWidget
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);

    NavigationBar *navigationBar() const;
    void setMaximized(bool maximized);
    void setAccount(const QString &name, const QString &typeText);
    void setAccountName(const QString &name);

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
    AvatarWidget *m_accountAvatar = nullptr;
    QPushButton *m_accountButton = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountType = nullptr;
    WindowButton *m_minimizeButton = nullptr;
    WindowButton *m_maximizeButton = nullptr;
    WindowButton *m_closeButton = nullptr;
};
