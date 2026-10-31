/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AccountManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUuid>

namespace {

const QString kSettingsOrganization = QStringLiteral("MysticTurtle");
const QString kSettingsApplication = QStringLiteral("MysticTurtle");
const QString kAccountsJsonKey = QStringLiteral("accounts/json");
const QString kActiveAccountKey = QStringLiteral("accounts/activeId");
const QString kLegacyAccountGroup = QStringLiteral("account");

bool validateUsername(const QString &username, QString *errorMessage)
{
    static const QRegularExpression allowed(
        QStringLiteral("^[\\p{L}\\p{N}_]{1,16}$"));
    if (username.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请输入用户名。");
        }
        return false;
    }
    if (!allowed.match(username).hasMatch()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("用户名需为 1–16 个字符，只能包含文字、数字或下划线。");
        }
        return false;
    }
    return true;
}

QString createAccountId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

bool PlayerAccount::isValid() const
{
    return !id.isEmpty() && !username.isEmpty();
}

QString PlayerAccount::typeLabel() const
{
    return type == Type::Premium ? QStringLiteral("正版账户")
                                 : QStringLiteral("离线账户");
}

AccountManager &AccountManager::instance()
{
    static AccountManager manager;
    return manager;
}

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
{
    load();
}

const QVector<PlayerAccount> &AccountManager::accounts() const
{
    return m_accounts;
}

PlayerAccount AccountManager::account() const
{
    const PlayerAccount *active = findAccount(m_activeId);
    return active ? *active : PlayerAccount();
}

PlayerAccount AccountManager::launchAccount() const
{
    return account();
}

QString AccountManager::activeAccountId() const
{
    return m_activeId;
}

bool AccountManager::hasAccount() const
{
    return findAccount(m_activeId) != nullptr;
}

bool AccountManager::addOfflineAccount(const QString &username,
                                       QString *errorMessage,
                                       QString *newAccountId)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (newAccountId) {
        newAccountId->clear();
    }

    const QString cleaned = username.trimmed();
    if (!validateUsername(cleaned, errorMessage)) {
        return false;
    }
    if (usernameExists(cleaned)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("账户列表中已经有这个用户名。");
        }
        return false;
    }

    PlayerAccount account;
    account.id = createAccountId();
    account.username = cleaned;
    account.type = PlayerAccount::Type::Offline;
    m_accounts.append(account);
    m_activeId = account.id;

    persist();
    emit accountChanged();
    if (newAccountId) {
        *newAccountId = account.id;
    }
    return true;
}

bool AccountManager::renameAccount(const QString &id, const QString &username,
                                   QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    PlayerAccount *account = findAccount(id);
    if (!account) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到这个账户。");
        }
        return false;
    }

    const QString cleaned = username.trimmed();
    if (!validateUsername(cleaned, errorMessage)) {
        return false;
    }
    if (usernameExists(cleaned, id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("账户列表中已经有这个用户名。");
        }
        return false;
    }

    account->username = cleaned;
    persist();
    emit accountChanged();
    return true;
}

bool AccountManager::removeAccount(const QString &id, QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    int index = -1;
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts.at(i).id == id) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到这个账户。");
        }
        return false;
    }

    const bool wasActive = m_activeId == id;
    m_accounts.removeAt(index);
    if (wasActive) {
        m_activeId = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
    }

    persist();
    emit accountChanged();
    return true;
}

bool AccountManager::setActiveAccount(const QString &id)
{
    if (!findAccount(id)) {
        return false;
    }
    if (m_activeId == id) {
        return true;
    }

    m_activeId = id;
    persist();
    emit accountChanged();
    return true;
}

const PlayerAccount *AccountManager::findAccount(const QString &id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    for (const PlayerAccount &account : m_accounts) {
        if (account.id == id) {
            return &account;
        }
    }
    return nullptr;
}

PlayerAccount *AccountManager::findAccount(const QString &id)
{
    if (id.isEmpty()) {
        return nullptr;
    }
    for (PlayerAccount &account : m_accounts) {
        if (account.id == id) {
            return &account;
        }
    }
    return nullptr;
}

bool AccountManager::usernameExists(const QString &username,
                                    const QString &exceptId) const
{
    for (const PlayerAccount &account : m_accounts) {
        if (account.id == exceptId) {
            continue;
        }
        if (account.username.compare(username, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void AccountManager::load()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       kSettingsOrganization, kSettingsApplication);

    const QString raw = settings.value(kAccountsJsonKey).toString();
    if (!raw.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(raw.toUtf8());
        if (document.isArray()) {
            QSet<QString> usedIds;
            QSet<QString> usedNames;
            for (const QJsonValue &value : document.array()) {
                if (!value.isObject()) {
                    continue;
                }
                const QJsonObject object = value.toObject();
                const QString username =
                    object.value(QStringLiteral("username")).toString().trimmed();
                if (username.isEmpty()) {
                    continue;
                }
                const QString nameKey = username.toLower();
                if (usedNames.contains(nameKey)) {
                    continue;
                }

                PlayerAccount account;
                account.id =
                    object.value(QStringLiteral("id")).toString().trimmed();
                if (account.id.isEmpty() || usedIds.contains(account.id)) {
                    account.id = createAccountId();
                }
                account.username = username;
                account.type =
                    object.value(QStringLiteral("type")).toString()
                            == QStringLiteral("premium")
                        ? PlayerAccount::Type::Premium
                        : PlayerAccount::Type::Offline;
                usedIds.insert(account.id);
                usedNames.insert(nameKey);
                m_accounts.append(account);
            }
        }
    }

    if (m_accounts.isEmpty()) {
        const QString legacyUsername =
            settings.value(kLegacyAccountGroup + QStringLiteral("/username"))
                .toString()
                .trimmed();
        if (!legacyUsername.isEmpty()) {
            PlayerAccount account;
            account.id = settings.value(kLegacyAccountGroup + QStringLiteral("/id"))
                             .toString()
                             .trimmed();
            if (account.id.isEmpty()) {
                account.id = createAccountId();
            }
            account.username = legacyUsername;
            account.type =
                settings.value(kLegacyAccountGroup + QStringLiteral("/type")).toString()
                        == QStringLiteral("premium")
                    ? PlayerAccount::Type::Premium
                    : PlayerAccount::Type::Offline;
            m_accounts.append(account);
            m_activeId = account.id;
            settings.remove(kLegacyAccountGroup);
            persist();
            return;
        }
    }

    m_activeId = settings.value(kActiveAccountKey).toString().trimmed();
    if (!findAccount(m_activeId)) {
        m_activeId = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
    }

    if (!raw.isEmpty()) {
        persist();
    }
}

void AccountManager::persist()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       kSettingsOrganization, kSettingsApplication);

    QJsonArray array;
    for (const PlayerAccount &account : m_accounts) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), account.id);
        object.insert(QStringLiteral("username"), account.username);
        object.insert(QStringLiteral("type"),
                      account.type == PlayerAccount::Type::Premium
                          ? QStringLiteral("premium")
                          : QStringLiteral("offline"));
        array.append(object);
    }

    settings.setValue(
        kAccountsJsonKey,
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    settings.setValue(kActiveAccountKey, m_activeId);
    settings.remove(kLegacyAccountGroup);
    settings.sync();
}
