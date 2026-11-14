/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QWidget>
#include <QStringList>

#include <functional>

class AccentButton;
class EasyTierProcess;
class QFile;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QTimer;

class MultiplayerPage : public QWidget
{
    Q_OBJECT

public:
    explicit MultiplayerPage(QWidget *parent = nullptr);
    ~MultiplayerPage() override;

    void setAnimationsEnabled(bool enabled);

signals:
    void createRoomRequested(const QString &roomName,
                             const QString &password,
                             const QString &virtualIp);
    void joinRoomRequested(const QString &roomCodeOrIp);

public slots:
    void setConnectionState(const QString &state);
    void setCreateStatus(const QString &status);
    void setJoinStatus(const QString &status);
    void setRoomCode(const QString &code);
    void appendLog(const QString &line);

private:
    void refreshRoomCode();
    void ensureEasyTier(std::function<void(bool)> callback);
    void startEasyTierDownload();
    void finishEasyTierArchive();
     void finishEnsure(bool success);
    void startCreateRoom(const QString &roomName, const QString &password,
                         const QString &virtualIp);
    void startJoinRoom(const QString &target);
    void handleEasyTierState(const QString &state);
    void setInputsEnabled(bool enabled);
    void resetConnectionUi(const QString &topState, const QString &createStatus,
                           const QString &joinStatus);
    QLineEdit *makeInput(const QString &placeholder, const QString &text,
                         QWidget *parent);
    QWidget *buildCreateCard();
    QWidget *buildJoinCard();
    QWidget *buildConsole();

    QLabel *m_connectionState = nullptr;
    QLineEdit *m_roomName = nullptr;
    QLineEdit *m_roomPassword = nullptr;
    QLineEdit *m_virtualIp = nullptr;
    QLineEdit *m_joinCode = nullptr;
    QLineEdit *m_hostIp = nullptr;
    QLineEdit *m_gamePort = nullptr;
    QLabel *m_createStatus = nullptr;
    QLabel *m_joinStatus = nullptr;
    QLabel *m_createHint = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTimer *m_connectTimeout = nullptr;
    AccentButton *m_createButton = nullptr;
    AccentButton *m_joinButton = nullptr;
    AccentButton *m_refreshButton = nullptr;
    AccentButton *m_copyIpButton = nullptr;
    AccentButton *m_connectHostButton = nullptr;
    EasyTierProcess *m_easyTier = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_easytierBinaryPath;
    QStringList m_easyTierDownloadUrls;
    QString m_easyTierDirectory;
    QString m_easyTierZipPath;
    int m_easyTierDownloadIndex = 0;
    std::function<void(bool)> m_ensureCallback;
    QString m_pendingRoomName;
    QString m_pendingRoomPassword;
    QString m_pendingJoinTarget;
    bool m_ensureInProgress = false;
    bool m_createMode = false;
    bool m_animationsEnabled = true;
};
