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

#include <QString>
#include <QVector>
#include <QWidget>

class AccentButton;
class AnimatedStackedWidget;
class AnimePortraitWidget;
class HomeNewsWidget;
class HomeStatusWidget;
class QLabel;
class QStackedWidget;
class TurtleAnimationWidget;
class VersionSelectPage;

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

    QString gameDirectory() const;
    VersionInfo versionAt(int versionIndex) const;
    VersionInfo selectedVersion() const;
    int selectedVersionIndex() const;
    void setAnimationsEnabled(bool enabled);

public slots:
    void reloadLocalVersions();
    void openAccountDialog();
    void setGameDirectory(const QString &directory);
    void refreshHomepageContent();

signals:
    void detailsRequested(int versionIndex);
    void notifyRequested(const QString &message);
    void gameDirectoryChanged(const QString &directory);
    void launchRequested();

private:
    void chooseVersion();
    void selectVersion(int index);
    void deleteVersion(int index);
    void updateSelectedVersionUi();

    QString m_gameDirectory;
    QVector<VersionInfo> m_versions;
    int m_selectedVersionIndex = -1;
    quint64 m_scanGeneration = 0;

    AnimatedStackedWidget *m_stack = nullptr;
    QWidget *m_overviewPage = nullptr;
    VersionSelectPage *m_versionPage = nullptr;
    QLabel *m_versionNameLabel = nullptr;
    QLabel *m_versionMetaLabel = nullptr;
    QLabel *m_versionStatsLabel = nullptr;
    QLabel *m_scanStatusLabel = nullptr;
    AccentButton *m_launchButton = nullptr;
    AccentButton *m_selectVersionButton = nullptr;
    QStackedWidget *m_newsStack = nullptr;
    HomeNewsWidget *m_newsWidget = nullptr;
    HomeStatusWidget *m_statusWidget = nullptr;
    QWidget *m_blankNewsPage = nullptr;
    TurtleAnimationWidget *m_turtleAnimation = nullptr;
    AnimePortraitWidget *m_animePortrait = nullptr;
};
