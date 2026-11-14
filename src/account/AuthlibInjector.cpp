/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AuthlibInjector.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace {

struct FetchResult
{
    bool ok = false;
    QByteArray data;
    QString error;
};

FetchResult fetch(const QUrl &url, int timeoutMs,
                  const std::function<bool()> &cancelled)
{
    FetchResult result;
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("MysticTurtle/3.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(timeoutMs);
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

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.data = reply->readAll();
    if (aborted) {
        result.error = QStringLiteral("已取消下载 authlib-injector。");
    } else if (timedOut) {
        result.error = QStringLiteral("连接 authlib-injector 下载服务器超时。");
    } else if (reply->error() != QNetworkReply::NoError || status < 200
               || status >= 400) {
        result.error = QStringLiteral("下载 authlib-injector 失败：HTTP %1。")
                           .arg(status);
    } else {
        result.ok = true;
    }
    reply->deleteLater();
    return result;
}

bool writeFile(const QString &path, const QByteArray &data, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入 %1：%2")
                                .arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace

QString AuthlibInjector::cachedJarPath()
{
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.mysticturtle");
    }
    return QDir(base).filePath(QStringLiteral("authlib-injector/authlib-injector.jar"));
}

QString AuthlibInjector::ensureJar(QString *errorMessage,
                                   const std::function<bool()> &cancelled)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const QString cached = cachedJarPath();
    if (QFileInfo(cached).isFile() && QFileInfo(cached).size() > 1024) {
        return cached;
    }

    const QStringList localCandidates = {
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("authlib-injector.jar")),
        QDir::current().filePath(QStringLiteral("authlib-injector.jar"))
    };
    for (const QString &candidate : localCandidates) {
        if (QFileInfo(candidate).isFile() && QFileInfo(candidate).size() > 1024) {
            return candidate;
        }
    }

    const QUrl metadataUrl(
        QStringLiteral("https://authlib-injector.yushi.moe/artifact/latest.json"));
    const FetchResult metadata = fetch(metadataUrl, 15000, cancelled);
    if (!metadata.ok) {
        if (errorMessage) {
            *errorMessage = metadata.error;
        }
        return QString();
    }

    const QJsonDocument json = QJsonDocument::fromJson(metadata.data);
    if (!json.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("authlib-injector 版本信息格式无效。");
        }
        return QString();
    }
    const QJsonObject object = json.object();
    QString downloadUrl =
        object.value(QStringLiteral("download_url")).toString();
    if (downloadUrl.isEmpty()) {
        downloadUrl = object.value(QStringLiteral("jar_url")).toString();
    }
    if (downloadUrl.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("没有获取到 authlib-injector 下载地址。");
        }
        return QString();
    }
    const QUrl jarUrl = metadataUrl.resolved(QUrl(downloadUrl));
    if (!jarUrl.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("authlib-injector 下载地址无效。");
        }
        return QString();
    }

    const FetchResult jar = fetch(jarUrl, 120000, cancelled);
    if (!jar.ok) {
        if (errorMessage) {
            *errorMessage = jar.error;
        }
        return QString();
    }
    if (jar.data.size() < 1024) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("下载到的 authlib-injector 文件无效。");
        }
        return QString();
    }
    if (!QDir().mkpath(QFileInfo(cached).absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建 authlib-injector 缓存目录。");
        }
        return QString();
    }
    if (!writeFile(cached, jar.data, errorMessage)) {
        return QString();
    }
    return cached;
}
