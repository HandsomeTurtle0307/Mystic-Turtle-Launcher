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
#include <QList>

class DownloadGamePage;
class DownloadModsPage;
class QButtonGroup;
class QStackedWidget;

class DownloadCenterPage : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadCenterPage(QWidget *parent = nullptr);

    DownloadGamePage *gamePage() const;
    DownloadModsPage *modsPage() const;
    void setGameDirectory(const QString &directory);
    void showModpacks();

signals:
    void notifyRequested(const QString &message);
    void modsChanged();

private:
    void connectProjectPage(DownloadModsPage *page);

    DownloadGamePage *m_gamePage = nullptr;
    DownloadModsPage *m_modsPage = nullptr;
    QList<DownloadModsPage *> m_projectPages;
    QButtonGroup *m_categoryGroup = nullptr;
    QStackedWidget *m_content = nullptr;
};
