/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QVector>

class QLabel;
class QComboBox;
class QJsonObject;
class QLineEdit;
class LoadingIndicator;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QShowEvent;
class QToolButton;
class DownloadTask;
class ModpackInstaller;
class VersionManifestClient;
class QUrl;
class QVBoxLayout;
class QProgressDialog;

class DownloadModsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadModsPage(const QString &projectType = QStringLiteral("mod"),
                              const QString &categoryName = QStringLiteral("Mod"),
                              QWidget *parent = nullptr);
    void setGameDirectory(const QString &directory);

signals:
    void notifyRequested(const QString &message);
    void modsChanged();

private:
    void showEvent(QShowEvent *event) override;
    void searchProjects();
    void fetchSearchPage();
    void handleSearchReply();
    void showProjectDetails(const QJsonObject &project);
    void clearResults();
    bool startDownload(const QUrl &url,
                       const QString &fileName,
                       const QByteArray &sha1,
                       const QString &modName);
    void setSearchStatus(const QString &message);
    bool startModpackInstall(const QUrl &url, const QString &fileName,
                             const QByteArray &sha1, const QString &displayName);

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_gameVersionCombo = nullptr;
    QComboBox *m_loaderCombo = nullptr;
    QLabel *m_searchStatus = nullptr;
    QLabel *m_versionLoadingLabel = nullptr;
    LoadingIndicator *m_versionLoadingIndicator = nullptr;
    QPushButton *m_retryVersionManifestButton = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_previousPageButton = nullptr;
    QPushButton *m_nextPageButton = nullptr;
    QWidget *m_resultsContent = nullptr;
    QWidget *m_loadingWidget = nullptr;
    QVBoxLayout *m_resultsLayout = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_downloadName = nullptr;
    QLabel *m_downloadSpeed = nullptr;
    QProgressBar *m_downloadProgress = nullptr;
    QPushButton *m_cancelModDownloadButton = nullptr;
    QWidget *m_downloadPanel = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    VersionManifestClient *m_manifestClient = nullptr;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<DownloadTask> m_downloadTask;
    QPointer<ModpackInstaller> m_modpackInstaller;
    QPointer<QProgressDialog> m_modpackProgress;
    QStringList m_gameVersionIds;
    QElapsedTimer m_speedClock;
    qint64 m_previousBytes = 0;
    int m_currentPage = 0;
    int m_totalHits = 0;
    bool m_initialSearchStarted = false;
    QString m_pendingLoadError;
    QString m_gameDirectory;
    QString m_projectType;
    QString m_categoryName;
};
