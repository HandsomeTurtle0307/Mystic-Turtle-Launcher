/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DownloadManager.h"

#include "DownloadTask.h"
#include "MojangMirror.h"

#include <QSet>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
}

QString DownloadManager::getOptimizedUrl(QString originalUrl)
{
    return MojangMirror::optimizedUrl(originalUrl);
}

bool DownloadManager::start(const QList<DownloadTask *> &tasks)
{
    if (m_running) {
        return false;
    }

    QSet<DownloadTask *> uniqueTasks;
    for (DownloadTask *task : tasks) {
        if (!task
            || task->isRunning()
            || task->isFinished()
            || task->thread() != thread()
            || uniqueTasks.contains(task)) {
            return false;
        }
        uniqueTasks.insert(task);
    }

    // A finished batch may still keep its idle task objects alive. Clear them
    // before accepting the next batch.
    for (DownloadTask *oldTask : m_tasks) {
        if (!oldTask) {
            continue;
        }
        oldTask->disconnect(this);
        delete oldTask;
    }
    m_tasks.clear();

    m_tasks = tasks;
    m_total = m_tasks.size();
    m_completed = 0;
    m_active = 0;
    m_nextIndex = 0;
    m_lastOverallPercent = 0;
    m_taskPercent = QVector<int>(m_total, 0);
    m_taskReceived = QVector<qint64>(m_total, 0);
    m_taskDone = QVector<bool>(m_total, false);
    m_totalReceived = 0;
    m_lastSpeedSampleBytes = 0;
    m_speedTimer.invalidate();
    m_running = m_total > 0;
    m_finished = false;

    for (int i = 0; i < m_tasks.size(); ++i) {
        DownloadTask *task = m_tasks.at(i);
        task->setParent(this);

        connect(task, &DownloadTask::progressChanged, this,
                [this, i](int percent) { handleTaskProgress(i, percent); });
        connect(task, &DownloadTask::bytesProgressChanged, this,
                [this, i](qint64 bytesReceived, qint64) {
                    handleTaskBytes(i, bytesReceived);
                });
        connect(task, &DownloadTask::succeeded, this,
                [this, i](const QString &filePath) {
                    handleTaskResult(i, true, filePath);
                });
        connect(task, &DownloadTask::failed, this,
                [this, i](const QString &reason) {
                    handleTaskResult(i, false, reason);
                });
    }

    emit overallProgress(0, m_total);
    emit overallPercentChanged(0);
    emit speedChanged(0);

    if (m_total == 0) {
        m_running = false;
        m_finished = true;
        emit finished();
        return true;
    }

    startNext();
    return true;
}

void DownloadManager::setMaxConcurrent(int count)
{
    const int clamped = qBound(1, count, 8);
    if (clamped == m_maxConcurrent) {
        return;
    }
    m_maxConcurrent = clamped;
    if (m_running) {
        startNext();
    }
}

int DownloadManager::maxConcurrent() const
{
    return m_maxConcurrent;
}

bool DownloadManager::isRunning() const
{
    return m_running;
}

bool DownloadManager::isFinished() const
{
    return m_finished;
}

int DownloadManager::totalCount() const
{
    return m_total;
}

int DownloadManager::completedCount() const
{
    return m_completed;
}

int DownloadManager::activeCount() const
{
    return m_active;
}

void DownloadManager::startNext()
{
    while (m_running
           && m_active < m_maxConcurrent
           && m_nextIndex < m_total) {
        const int index = m_nextIndex++;
        DownloadTask *task = m_tasks.at(index);
        if (!task || m_taskDone.at(index)) {
            continue;
        }

        ++m_active;
        emit taskStarted(index);
        task->start();
    }
}

void DownloadManager::handleTaskProgress(int index, int percent)
{
    if (!m_running
        || index < 0
        || index >= m_total
        || m_taskDone.at(index)) {
        return;
    }

    m_taskPercent[index] = qBound(0, percent, 100);
    emit taskProgressChanged(index, percent);
    updateOverallPercent();
}

void DownloadManager::handleTaskBytes(int index, qint64 bytesReceived)
{
    if (!m_running
        || index < 0
        || index >= m_total
        || m_taskDone.at(index)) {
        return;
    }

    const qint64 received = qMax<qint64>(0, bytesReceived);
    m_totalReceived += received - m_taskReceived.at(index);
    m_taskReceived[index] = received;

    if (!m_speedTimer.isValid()) {
        m_speedTimer.start();
        m_lastSpeedSampleBytes = m_totalReceived;
        return;
    }

    const qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed < 350) {
        return;
    }

    const qint64 delta = qMax<qint64>(0, m_totalReceived - m_lastSpeedSampleBytes);
    const qint64 speed = (delta * 1000) / elapsed;
    m_speedTimer.restart();
    m_lastSpeedSampleBytes = m_totalReceived;
    emit speedChanged(speed);
}

void DownloadManager::handleTaskResult(int index,
                                       bool success,
                                       const QString &message)
{
    if (!m_running
        || index < 0
        || index >= m_total
        || m_taskDone.at(index)) {
        return;
    }

    m_taskDone[index] = true;
    m_taskPercent[index] = success ? 100 : 0;
    --m_active;
    ++m_completed;

    if (success) {
        emit taskSucceeded(index, message);
    } else {
        emit taskFailed(index, message);
    }

    emit overallProgress(m_completed, m_total);
    updateOverallPercent();
    startNext();

    // The result has been delivered; release the completed task and its idle
    // worker thread instead of keeping every finished download alive.
    if (DownloadTask *finishedTask = m_tasks.at(index)) {
        m_tasks[index] = nullptr;
        finishedTask->deleteLater();
    }

    if (m_completed >= m_total && m_active == 0) {
        m_running = false;
        m_finished = true;
        m_speedTimer.invalidate();
        emit speedChanged(0);
        emit finished();
    }
}

void DownloadManager::updateOverallPercent()
{
    if (m_total <= 0) {
        return;
    }

    qint64 sum = 0;
    for (int percent : m_taskPercent) {
        sum += qBound(0, percent, 100);
    }
    const int percent = qBound(
        0, static_cast<int>(qRound(static_cast<double>(sum) / m_total)), 100);

    // Rounding may reach 100 while the last task is still hashing or
    // finishing. Keep the aggregate below 100 until every task has a result.
    const int displayedPercent =
        (m_completed < m_total) ? qMin(percent, 99) : percent;

    if (displayedPercent == m_lastOverallPercent) {
        return;
    }

    m_lastOverallPercent = displayedPercent;
    emit overallPercentChanged(displayedPercent);
}
