/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "LoaderDownloader.h"
#include "VersionManifest.h"

#include <QList>
#include <QString>
#include <QWidget>

class AccentButton;
class CardWidget;
class GameInstaller;
class LoaderSetupView;
class QGridLayout;
class QLabel;
class QProgressBar;
class QScrollArea;
class QStackedWidget;
class SearchField;
class VersionManifestClient;

class DownloadGamePage : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadGamePage(QWidget *parent = nullptr);

    void setGameDirectory(const QString &directory);

signals:
    void notifyRequested(const QString &message);
    void installSucceeded(const QString &versionId);
    void baseInstallFinished(const QString &versionId);

private:
    void fetchManifest();
    void handleManifestReady(const MinecraftVersionManifest &manifest);
    void handleManifestFailed(const QString &reason);
    void refreshCards();
    void openLoaderSetup(const QString &version);
    void beginSelectedDownload();
    void startLoaderDownload();
    void finalizeDownload(const QString &summary);
    void handleLoaderFailed(const QString &reason);
    void startInstall(const QString &version);
    void updateInstallButtonsEnabled();
    void updateFeatured();
    void updatePageControls(int totalPages);

    QStackedWidget *m_views = nullptr;
    QWidget *m_listPage = nullptr;
    LoaderSetupView *m_loaderView = nullptr;
    LoaderDownloader *m_loaderDownloader = nullptr;
    LoaderSelection m_loaderSelection;
    QString m_baseVersion;
    QUrl m_baseJsonUrl;
    QString m_baseSha1;
    bool m_loaderDownloadActive = false;

    SearchField *m_search = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_manifestStatusLabel = nullptr;
    AccentButton *m_retryManifestButton = nullptr;
    QString m_filter = QStringLiteral("全部");

    VersionManifestClient *m_manifestClient = nullptr;
    QVector<MinecraftVersionEntry> m_versions;
    QString m_latestReleaseId;
    QString m_latestSnapshotId;
    bool m_manifestLoading = false;
    int m_page = 0;
    static constexpr int kPageSize = 20;

    GameInstaller *m_installer = nullptr;
    CardWidget *m_progressPanel = nullptr;
    QLabel *m_progressTitle = nullptr;
    QLabel *m_progressPercent = nullptr;
    QLabel *m_progressSpeed = nullptr;
    QLabel *m_progressDetail = nullptr;
    QProgressBar *m_progressBar = nullptr;
    AccentButton *m_featuredInstallButton = nullptr;
    QLabel *m_featuredVersionLabel = nullptr;
    QLabel *m_featuredMetaLabel = nullptr;
    QString m_featuredVersionId;
    QUrl m_featuredVersionUrl;
    QString m_featuredVersionSha1;
    QList<AccentButton *> m_installButtons;
    QLabel *m_pageLabel = nullptr;
    AccentButton *m_prevPageButton = nullptr;
    AccentButton *m_nextPageButton = nullptr;
    QScrollArea *m_versionScroll = nullptr;
    bool m_installing = false;
};
