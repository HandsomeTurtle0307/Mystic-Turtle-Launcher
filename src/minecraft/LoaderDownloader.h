/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QUrl>

class DownloadManager;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;

struct LoaderSelection
{
    QString loader;
    QString loaderVersion;
    QString downloadUrl;
    QString downloadSha1;
    QString downloadFilename;

    QString fabricApiVersion;
    QString fabricApiUrl;
    QString fabricApiSha1;
    QString fabricApiFilename;

    bool hasLoader() const { return !loader.isEmpty(); }
};

// Downloads add-ons on top of an installed base version.
//
// Fabric produces a complete launcher profile plus loader libraries and the
// selected Fabric API jar. Forge and NeoForge installer jars are downloaded
// into <gameDir>/installers and then executed automatically.
// OptiFine currently only downloads its official installer because it does not
// expose a reliable headless command-line install mode.
class LoaderDownloader : public QObject
{
    Q_OBJECT

public:
    explicit LoaderDownloader(const QString &gameDirectory, QObject *parent = nullptr);

    void start(const QString &baseVersion, const LoaderSelection &selection);
    bool isBusy() const;

signals:
    void phaseChanged(const QString &phase);
    void percentChanged(int percent);
    void speedChanged(qint64 bytesPerSecond);
    void finished(const QString &summary);
    void failed(const QString &reason);

private:
    void fetchFabricProfile();
    void handleFabricProfile();
    void startFabricDownloads(const QByteArray &profileJson);
    void downloadInstaller();
    void handleDownloadsFinished();
    void startInstaller();
    void handleInstallerFinished(int exitCode, QProcess::ExitStatus status);
    void handleInstallerOutput();
    void cleanupInstallerTemp();
    QString findInstalledVersionId() const;
    QString installerLogTail() const;
    void fail(const QString &reason);
    QString mavenRelativePath(const QString &coordinates) const;

    QNetworkAccessManager *m_network = nullptr;
    DownloadManager *m_manager = nullptr;
    QPointer<QNetworkReply> m_profileReply;
    QPointer<QProcess> m_installerProcess;
    QTimer *m_installerTimeout = nullptr;

    QString m_gameDirectory;
    QString m_baseVersion;
    LoaderSelection m_selection;
    QString m_fabricVersionId;
    QString m_installerPath;
    QString m_installerTempHome;
    QString m_installerLog;
    int m_failures = 0;
    QString m_lastFailure;
    bool m_busy = false;
};
