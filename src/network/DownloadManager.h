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
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class DownloadTask;

// Schedules DownloadTask instances with a hard concurrency cap of 8.
//
// Ownership of every task passed to a successful start() is transferred to the
// manager. Tasks must be fresh (not running and not finished) and are expected
// to live in the same thread as the manager. Tasks are started in list order.
// A task is deleted automatically after its success or failure result signal.
class DownloadManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadManager)

public:
    explicit DownloadManager(QObject *parent = nullptr);

    // Returns a BMCLAPI URL for Mojang launcher/download/meta hosts or a
    // libraries.minecraft.net Maven URL, or the original URL for any other
    // host.
    static QString getOptimizedUrl(QString originalUrl);

    // Starts a new batch. Returns false if a batch is already running or any
    // task is null, duplicated, already running, or already finished.
    bool start(const QList<DownloadTask *> &tasks);

    // Allowed range is 1..8. The default is 8. Lowering the limit does not
    // cancel active downloads; new tasks wait until the active count drops.
    void setMaxConcurrent(int count);
    int maxConcurrent() const;

    bool isRunning() const;
    bool isFinished() const;

    int totalCount() const;
    int completedCount() const;
    int activeCount() const;

signals:
    // completedTasks grows from 0 to totalTasks. Useful for a task-count
    // progress display.
    void overallProgress(int completedTasks, int totalTasks);

    // Smooth aggregate of each task's progressChanged value (0..100).
    void overallPercentChanged(int percent);

    void taskStarted(int index);
    void taskProgressChanged(int index, int percent);
    void taskSucceeded(int index, const QString &filePath);
    void taskFailed(int index, const QString &reason);

    // Aggregate transfer speed sampled from all active tasks.
    void speedChanged(qint64 bytesPerSecond);

    void finished();

private:
    void startNext();
    void handleTaskProgress(int index, int percent);
    void handleTaskBytes(int index, qint64 bytesReceived);
    void handleTaskResult(int index, bool success, const QString &message);
    void updateOverallPercent();

    QList<DownloadTask *> m_tasks;
    QVector<int> m_taskPercent;
    QVector<qint64> m_taskReceived;
    QVector<bool> m_taskDone;
    QElapsedTimer m_speedTimer;
    qint64 m_totalReceived = 0;
    qint64 m_lastSpeedSampleBytes = 0;
    int m_total = 0;
    int m_completed = 0;
    int m_active = 0;
    int m_nextIndex = 0;
    int m_maxConcurrent = 8;
    int m_lastOverallPercent = -1;
    bool m_running = false;
    bool m_finished = false;
};
