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
#include <QProcess>
#include <QString>
#include <QStringList>

class QTimer;

class EasyTierProcess : public QObject
{
    Q_OBJECT

public:
    explicit EasyTierProcess(QObject *parent = nullptr);
    ~EasyTierProcess() override;

    void setBinaryPath(const QString &path);
    QString binaryPath() const;

    bool isRunning() const;
    void startCreate(const QString &roomName, const QString &password,
                     const QString &virtualIp);
    void startJoin(const QString &networkName, const QString &password);
    void stop();

signals:
    void logLine(const QString &line);
    void stateChanged(const QString &state);
    void errorOccurred(const QString &message);

private:
    void startWithArguments(const QStringList &arguments);
    void readOutput();
    void handleLine(const QString &line);
    void setState(const QString &state);
    void attachJobObject();
    void queryPeers();
    void stopPeerMonitor();

    QProcess m_process;
    QProcess *m_cliProcess = nullptr;
    QTimer *m_peerTimer = nullptr;
    QString m_binaryPath;
    QString m_state = QStringLiteral("disconnected");
    QString m_localVirtualIp;
    bool m_stopping = false;
    bool m_connectedEmitted = false;
    bool m_cliAvailable = true;
    bool m_cliErrorLogged = false;
#ifdef Q_OS_WIN
    void *m_jobHandle = nullptr;
#endif
};
