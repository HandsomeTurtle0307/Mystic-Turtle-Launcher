/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#pragma once

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QStringList>

class DownloadManager;
class DownloadTask;
class GameInstaller;
class LoaderDownloader;
class QTemporaryDir;

class ModpackInstaller : public QObject
{
    Q_OBJECT

public:
    explicit ModpackInstaller(QString gameDirectory, QObject *parent = nullptr);

    void start(const QUrl &url, const QString &fileName, const QString &sha1,
               const QString &displayName);
    bool isBusy() const;
    void cancel();

signals:
    void phaseChanged(const QString &phase);
    void percentChanged(int percent);
    void modProgressChanged(int completed, int total);
    void finished(const QString &versionId);
    void failed(const QString &reason);
    void cancelled();

private:
    enum class Phase {
        Idle,
        DownloadingPack,
        Extracting,
        InstallingGame,
        InstallingLoader,
        DownloadingFiles,
        CreatingInstance
    };

    void handlePackDownloaded(const QString &path);
    void handleExtractFinished(int exitCode, QProcess::ExitStatus status);
    void parseIndexAndInstall();
    void installBaseGame();
    void installLoader();
    void downloadModFiles();
    void createInstance();
    bool copyOverrides(const QString &sourceDirectory,
                       const QString &targetDirectory, QString *errorMessage);
    void fail(const QString &reason);
    void reset();
    void finishCancelled();
    QString instanceVersionId() const;

    QString m_gameDirectory;
    QString m_displayName;
    QString m_minecraftVersion;
    QString m_loaderName;
    QString m_loaderVersion;
    QString m_installedLoaderId;
    QString m_overrideDirectory;
    QString m_indexPath;
    QString m_stagingModsDirectory;
    QString m_versionId;
    QString m_versionDirectory;
    QTemporaryDir *m_temporaryDirectory = nullptr;
    QPointer<DownloadTask> m_packTask;
    DownloadManager *m_manager = nullptr;
    QList<DownloadTask *> m_pendingDownloads;
    GameInstaller *m_gameInstaller = nullptr;
    LoaderDownloader *m_loaderDownloader = nullptr;
    QPointer<QProcess> m_extractProcess;
    Phase m_phase = Phase::Idle;
    bool m_busy = false;
    bool m_cancelled = false;
    int m_downloadFailures = 0;
    QString m_lastFailure;
    QStringList m_downloadPaths;
    QList<qint64> m_expectedSizes;
    QList<QByteArray> m_expectedSha512;
};
