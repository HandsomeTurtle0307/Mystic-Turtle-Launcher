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

class QLineEdit;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    void setGameDirectory(const QString &directory);
    void setJavaPath(const QString &path);

signals:
    void acrylicEnabledChanged(bool enabled);
    void animationsEnabledChanged(bool enabled);
    void backgroundChanged(const QString &imagePath, int opacity);
    void gameDirectoryChanged(const QString &directory);
    void notifyRequested(const QString &message);

private:
    void setBackgroundImage(const QString &imagePath);

    QLineEdit *m_gameDirEdit = nullptr;
    QLineEdit *m_javaEdit = nullptr;
    QLineEdit *m_backgroundPathEdit = nullptr;
    int m_backgroundOpacity = 35;
};
