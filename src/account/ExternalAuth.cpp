/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "ExternalAuth.h"

#include "AccountManager.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kAuthTimeoutMs = 15000;

struct HttpResult
{
    bool ok = false;
    QJsonObject body;
    int status = 0;
    QString error;
};

QUrl endpoint(const QUrl &root, const QString &path)
{
    QUrl url = root;
    QString basePath = url.path();
    while (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    url.setPath(basePath + path);
    return url;
}

HttpResult postJson(const QUrl &url, const QJsonObject &body,
                    const std::function<bool()> &cancelled)
{
    HttpResult result;
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("MysticTurtle/3.0"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = manager.post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(kAuthTimeoutMs);
    bool timedOut = false;
    bool aborted = false;
    QTimer cancelPoll;
    cancelPoll.setInterval(120);

    QObject::connect(&timeout, &QTimer::timeout, reply, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(&cancelPoll, &QTimer::timeout, reply, [&]() {
        if (cancelled && cancelled()) {
            aborted = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeout.start();
    if (cancelled) {
        cancelPoll.start();
    }
    if (cancelled && cancelled()) {
        aborted = true;
        reply->abort();
    }
    if (reply->isFinished()) {
        loop.quit();
    } else {
        loop.exec();
    }
    timeout.stop();
    cancelPoll.stop();

    result.status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (document.isObject()) {
        result.body = document.object();
    }

    if (aborted) {
        result.error = QStringLiteral("已取消外置登录验证。");
    } else if (timedOut) {
        result.error = QStringLiteral("认证服务器响应超时。");
    } else if (result.status >= 200 && result.status < 300
               && !result.body.isEmpty()) {
        result.ok = true;
    } else {
        result.error = result.body.value(QStringLiteral("errorMessage")).toString();
        if (result.error.isEmpty()) {
            result.error = result.body.value(QStringLiteral("message")).toString();
        }
        if (result.error.isEmpty()) {
            result.error = QStringLiteral("认证服务器返回 HTTP %1。").arg(result.status);
        }
    }

    reply->deleteLater();
    return result;
}

bool applySession(PlayerAccount *account, const QJsonObject &body,
                  QString *errorMessage)
{
    const QString accessToken =
        body.value(QStringLiteral("accessToken")).toString().trimmed();
    if (accessToken.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("认证服务器没有返回 accessToken。");
        }
        return false;
    }

    QJsonObject profile =
        body.value(QStringLiteral("selectedProfile")).toObject();
    if (profile.isEmpty()) {
        const QJsonArray profiles =
            body.value(QStringLiteral("availableProfiles")).toArray();
        if (!profiles.isEmpty()) {
            profile = profiles.first().toObject();
        }
    }

    account->accessToken = accessToken;
    const QString clientToken =
        body.value(QStringLiteral("clientToken")).toString().trimmed();
    if (!clientToken.isEmpty()) {
        account->clientToken = clientToken;
    }
    if (!profile.isEmpty()) {
        QString uuid = profile.value(QStringLiteral("id")).toString().trimmed();
        uuid.remove(QLatin1Char('-'));
        if (!uuid.isEmpty()) {
            account->uuid = uuid;
        }
        const QString name =
            profile.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            account->username = name;
        }
    }
    return true;
}

} // namespace

QUrl ExternalAuth::apiRoot(const QString &serverUrl)
{
    QString text = serverUrl.trimmed();
    if (text.isEmpty()) {
        return QUrl();
    }
    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }

    QUrl url(text);
    if (!url.isValid() || url.host().isEmpty()) {
        return QUrl();
    }
    url.setQuery(QString());
    url.setFragment(QString());

    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.endsWith(QStringLiteral("/authserver"))) {
        path.chop(QStringLiteral("/authserver").size());
    }
    if (path.isEmpty()) {
        path = QStringLiteral("/api/yggdrasil");
    }
    url.setPath(path);
    return url;
}

bool ExternalAuth::authenticate(PlayerAccount *account, QString *errorMessage,
                                const std::function<bool()> &cancelled)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!account || account->type != PlayerAccount::Type::External) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前账户不是第三方外置登录账户。");
        }
        return false;
    }

    const QUrl root = apiRoot(account->serverUrl);
    if (!root.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("外置登录认证服务器地址无效。");
        }
        return false;
    }

    // Saved Yggdrasil accounts normally continue with a token refresh. If no
    // usable token exists, fall back to the authenticate endpoint.
    if (!account->accessToken.isEmpty()
        && !account->clientToken.isEmpty()) {
        QJsonObject refreshBody;
        refreshBody.insert(QStringLiteral("accessToken"), account->accessToken);
        refreshBody.insert(QStringLiteral("clientToken"), account->clientToken);
        refreshBody.insert(QStringLiteral("requestUser"), true);
        const HttpResult refreshed =
            postJson(endpoint(root, QStringLiteral("/authserver/refresh")),
                     refreshBody, cancelled);
        if (refreshed.ok && applySession(account, refreshed.body, errorMessage)) {
            account->serverUrl = root.toString();
            return true;
        }
    }

    QJsonObject authBody;
    authBody.insert(QStringLiteral("username"), account->username);
    authBody.insert(QStringLiteral("password"), account->accessToken);
    authBody.insert(QStringLiteral("clientToken"), account->clientToken);
    authBody.insert(QStringLiteral("requestUser"), true);
    QJsonObject agent;
    agent.insert(QStringLiteral("name"), QStringLiteral("Minecraft"));
    agent.insert(QStringLiteral("version"), 1);
    authBody.insert(QStringLiteral("agent"), agent);

    const HttpResult authenticated =
        postJson(endpoint(root, QStringLiteral("/authserver/authenticate")),
                 authBody, cancelled);
    if (!authenticated.ok) {
        if (errorMessage) {
            *errorMessage = authenticated.error;
        }
        return false;
    }
    if (!applySession(account, authenticated.body, errorMessage)) {
        return false;
    }
    account->serverUrl = root.toString();
    return true;
}
