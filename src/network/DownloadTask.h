/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

class DownloadWorker;
class QThread;

// One-shot, self-threaded file download task.
//
// DownloadTask creates its own worker thread. Connect the signals before
// calling start(). The task does not delete itself: connect finished() to
// deleteLater() when you no longer need it.
//
// Network failures are retried up to three times (30 second transfer timeout).
// Failed attempts restart from byte 0; resume is not implemented yet.
// launcher.mojang.com / piston-data.mojang.com / piston-meta.mojang.com /
// launchermeta.mojang.com URLs are automatically routed through BMCLAPI;
// libraries.minecraft.net is routed through BMCLAPI's Maven mirror. After two
// consecutive BMCLAPI timeouts, the task switches back to the original
// official URL and retries there.
// A SHA1 mismatch is never retried; the downloaded file is deleted immediately.
// An empty expected SHA1 skips verification (used by legacy library entries
// that do not publish hashes).
class DownloadTask : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadTask)

public:
    explicit DownloadTask(const QUrl &url,
                          const QString &saveFilePath,
                          const QByteArray &expectedSha1,
                          QObject *parent = nullptr);
    ~DownloadTask() override;

    QUrl url() const;
    QString saveFilePath() const;
    QByteArray expectedSha1() const;

    bool isRunning() const;
    bool isFinished() const;

public slots:
    void start();
    void cancel();

signals:
    // Emitted from the task's owning thread.
    void progressChanged(int percent);
    void bytesProgressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void succeeded(const QString &filePath);
    void failed(const QString &reason);
    void finished();

private:
    void cleanupThread();

    QUrl m_url;
    QString m_saveFilePath;
    QByteArray m_expectedSha1;
    QThread *m_thread = nullptr;
    DownloadWorker *m_worker = nullptr;
    bool m_started = false;
    bool m_finished = false;
};
