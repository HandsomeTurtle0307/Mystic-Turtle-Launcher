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
#include <QUrl>
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

bool validateExternalAccount(const PlayerAccount &account, QString *errorMessage)
{
    QString text = account.serverUrl.trimmed();
    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }
    const QUrl url(text);
    if (account.serverUrl.trimmed().isEmpty()
        || !url.isValid() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请输入有效的认证服务器地址。");
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
    return type == Type::External ? QStringLiteral("第三方外置登录")
                                  : QStringLiteral("离线账户");
}

QString PlayerAccount::serverLabel() const
{
    if (type != Type::External || serverUrl.trimmed().isEmpty()) {
        return QString();
    }
    const QUrl url(serverUrl.contains(QStringLiteral("://"))
                       ? serverUrl
                       : QStringLiteral("https://") + serverUrl);
    const QString host = url.host().trimmed();
    if (host.contains(QStringLiteral("littleskin"), Qt::CaseInsensitive)) {
        return QStringLiteral("LittleSkin");
    }
    return host.isEmpty() ? serverUrl : host;
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

bool AccountManager::addAccount(const PlayerAccount &account,
                                QString *errorMessage,
                                QString *newAccountId)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (newAccountId) {
        newAccountId->clear();
    }

    PlayerAccount candidate = account;
    candidate.username = candidate.username.trimmed();
    if (!validateUsername(candidate.username, errorMessage)) {
        return false;
    }
    if (usernameExists(candidate.username)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("账户列表中已经有这个用户名。");
        }
        return false;
    }
    if (candidate.type == PlayerAccount::Type::External
        && !validateExternalAccount(candidate, errorMessage)) {
        return false;
    }

    candidate.id = createAccountId();
    candidate.serverUrl = candidate.serverUrl.trimmed();
    candidate.accessToken = candidate.accessToken.trimmed();
    candidate.uuid = candidate.uuid.trimmed();
    candidate.clientToken = candidate.clientToken.trimmed();
    m_accounts.append(candidate);
    m_activeId = candidate.id;

    persist();
    emit accountChanged();
    if (newAccountId) {
        *newAccountId = candidate.id;
    }
    return true;
}

bool AccountManager::updateAccount(const QString &id, const PlayerAccount &account,
                                   QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    PlayerAccount *target = findAccount(id);
    if (!target) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到这个账户。");
        }
        return false;
    }

    PlayerAccount candidate = account;
    candidate.username = candidate.username.trimmed();
    if (!validateUsername(candidate.username, errorMessage)) {
        return false;
    }
    if (usernameExists(candidate.username, id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("账户列表中已经有这个用户名。");
        }
        return false;
    }
    if (candidate.type == PlayerAccount::Type::External
        && !validateExternalAccount(candidate, errorMessage)) {
        return false;
    }

    candidate.id = id;
    candidate.serverUrl = candidate.serverUrl.trimmed();
    candidate.accessToken = candidate.accessToken.trimmed();
    candidate.uuid = candidate.uuid.trimmed();
    candidate.clientToken = candidate.clientToken.trimmed();
    *target = candidate;

    persist();
    emit accountChanged();
    return true;
}

bool AccountManager::updateExternalSession(const QString &id,
                                           const QString &accessToken,
                                           const QString &clientToken,
                                           const QString &uuid,
                                           const QString &username,
                                           QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    PlayerAccount *account = findAccount(id);
    if (!account || account->type != PlayerAccount::Type::External) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到这个外置登录账户。");
        }
        return false;
    }

    if (!accessToken.trimmed().isEmpty()) {
        account->accessToken = accessToken.trimmed();
    }
    if (!clientToken.trimmed().isEmpty()) {
        account->clientToken = clientToken.trimmed();
    }
    if (!uuid.trimmed().isEmpty()) {
        account->uuid = uuid.trimmed();
    }
    if (!username.trimmed().isEmpty()) {
        account->username = username.trimmed();
    }
    persist();
    emit accountChanged();
    return true;
}

bool AccountManager::addOfflineAccount(const QString &username,
                                       QString *errorMessage,
                                       QString *newAccountId)
{
    PlayerAccount account;
    account.username = username;
    account.type = PlayerAccount::Type::Offline;
    return addAccount(account, errorMessage, newAccountId);
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
                const QString accountType =
                    object.value(QStringLiteral("accountType")).toString();
                const QString legacyType =
                    object.value(QStringLiteral("type")).toString();
                account.type =
                    accountType == QStringLiteral("external")
                        || legacyType == QStringLiteral("external")
                        ? PlayerAccount::Type::External
                        : PlayerAccount::Type::Offline;
                account.serverUrl =
                    object.value(QStringLiteral("serverUrl")).toString().trimmed();
                account.accessToken =
                    object.value(QStringLiteral("accessToken")).toString().trimmed();
                account.uuid =
                    object.value(QStringLiteral("uuid")).toString().trimmed();
                account.clientToken =
                    object.value(QStringLiteral("clientToken")).toString().trimmed();
                if (account.type == PlayerAccount::Type::External
                    && account.serverUrl.isEmpty()) {
                    // A previous premium marker has no auth server information.
                    account.type = PlayerAccount::Type::Offline;
                }
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
            account.type = PlayerAccount::Type::Offline;
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
        object.insert(QStringLiteral("accountType"),
                      account.type == PlayerAccount::Type::External
                          ? QStringLiteral("external")
                          : QStringLiteral("offline"));
        object.insert(QStringLiteral("serverUrl"), account.serverUrl);
        object.insert(QStringLiteral("accessToken"), account.accessToken);
        object.insert(QStringLiteral("uuid"), account.uuid);
        object.insert(QStringLiteral("clientToken"), account.clientToken);
        array.append(object);
    }

    settings.setValue(
        kAccountsJsonKey,
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    settings.setValue(kActiveAccountKey, m_activeId);
    settings.remove(kLegacyAccountGroup);
    settings.sync();
}
