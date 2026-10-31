/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "VersionParser.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class DownloadManager;
class QNetworkAccessManager;
class QNetworkReply;

// Downloads the base files for one Minecraft version:
//   version JSON, client jar, library jars, Windows natives, asset index and
//   all missing asset objects required by the index.
class GameInstaller : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GameInstaller)

public:
    explicit GameInstaller(QString gameDirectory, QObject *parent = nullptr);

    QString gameDirectory() const;
    void setGameDirectory(const QString &directory);

    bool isBusy() const;

    // Skips the manifest lookup when the caller already has the version JSON
    // URL and SHA1 (for example from the version list on the download page).
    void installFromManifest(const QString &versionId, const QUrl &versionJsonUrl,
                             const QString &versionJsonSha1);

public slots:
    void install(const QString &versionId);

signals:
    void phaseChanged(const QString &text);
    void overallProgress(int completedTasks, int totalTasks);
    void overallPercentChanged(int percent);
    void speedChanged(qint64 bytesPerSecond);
    void succeeded(const QString &versionId);
    void failed(const QString &reason);

private:
    enum class Phase {
        Idle,
        FetchingManifest,
        DownloadingVersionJson,
        DownloadingFiles,
        DownloadingAssets
    };

    void fetchManifest();
    void handleManifestReply();
    void downloadVersionJson();
    void downloadGameFiles(const VersionParser::VersionInfo &version);
    void downloadAssets();
    void handleManagerFinished();
    void fail(const QString &reason);
    void finishSuccess();
    void resetBatchTracking();
    QString versionJsonPath() const;

    QNetworkAccessManager *m_network = nullptr;
    DownloadManager *m_manager = nullptr;
    QPointer<QNetworkReply> m_manifestReply;

    QString m_gameDirectory;
    QString m_versionId;
    QUrl m_versionJsonUrl;
    QByteArray m_versionJsonSha1;
    QString m_versionJsonPath;
    QString m_assetIndexPath;

    Phase m_phase = Phase::Idle;
    bool m_busy = false;
    int m_batchFailures = 0;
    QString m_lastFailure;
};
