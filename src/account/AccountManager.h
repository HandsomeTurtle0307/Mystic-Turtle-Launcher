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
#include <QString>
#include <QVector>

struct PlayerAccount
{
    enum class Type {
        Offline,
        External
    };

    QString id;
    QString username;
    Type type = Type::Offline;

    // Only used by external (authlib-injector / Yggdrasil) accounts.
    QString serverUrl;
    QString accessToken;
    QString uuid;
    QString clientToken;

    bool isValid() const;
    QString typeLabel() const;
    QString serverLabel() const;
};

class AccountManager : public QObject
{
    Q_OBJECT

public:
    static AccountManager &instance();

    const QVector<PlayerAccount> &accounts() const;
    PlayerAccount account() const;
    PlayerAccount launchAccount() const;
    QString activeAccountId() const;
    bool hasAccount() const;

    bool addAccount(const PlayerAccount &account,
                    QString *errorMessage = nullptr,
                    QString *newAccountId = nullptr);
    bool updateAccount(const QString &id, const PlayerAccount &account,
                       QString *errorMessage = nullptr);
    bool updateExternalSession(const QString &id, const QString &accessToken,
                               const QString &clientToken, const QString &uuid,
                               const QString &username = QString(),
                               QString *errorMessage = nullptr);
    bool addOfflineAccount(const QString &username,
                           QString *errorMessage = nullptr,
                           QString *newAccountId = nullptr);
    bool renameAccount(const QString &id, const QString &username,
                       QString *errorMessage = nullptr);
    bool removeAccount(const QString &id, QString *errorMessage = nullptr);
    bool setActiveAccount(const QString &id);

signals:
    void accountChanged();

private:
    explicit AccountManager(QObject *parent = nullptr);

    void load();
    void persist();

    const PlayerAccount *findAccount(const QString &id) const;
    PlayerAccount *findAccount(const QString &id);
    bool usernameExists(const QString &username, const QString &exceptId = QString()) const;

    QVector<PlayerAccount> m_accounts;
    QString m_activeId;
};
