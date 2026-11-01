/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionManifest.h"

#include "MojangMirror.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {

const QString kOfficialManifestUrl =
    QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
const QString kMirrorManifestUrl =
    QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json");

} // namespace

QString versionCategory(const QString &type)
{
    if (type == QStringLiteral("release")) {
        return QStringLiteral("正式版");
    }
    if (type == QStringLiteral("snapshot")) {
        return QStringLiteral("快照版");
    }
    if (type == QStringLiteral("old_beta") || type == QStringLiteral("old_alpha")) {
        return QStringLiteral("远古版");
    }
    return QString();
}

VersionManifestClient::VersionManifestClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void VersionManifestClient::fetch()
{
    if (m_reply) {
        disconnect(m_reply.data(), nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    m_fetching = true;
    m_usingMirror = false;
    m_fallbackTried = false;
    requestManifest(
        MojangMirror::downloadSource() == MojangMirror::DownloadSource::Bmclapi);
}

bool VersionManifestClient::isFetching() const
{
    return m_fetching;
}

void VersionManifestClient::requestManifest(bool useMirror)
{
    if (!m_fetching) {
        return;
    }

    m_usingMirror = useMirror;
    QNetworkRequest request{QUrl(useMirror ? kMirrorManifestUrl : kOfficialManifestUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MysticTurtle/0.2"));
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_network->get(request);
    if (!reply) {
        if (!m_fallbackTried) {
            m_fallbackTried = true;
            requestManifest(!useMirror);
        } else {
            m_fetching = false;
            emit failed(QStringLiteral("无法发起版本清单请求。"));
        }
        return;
    }

    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, &VersionManifestClient::handleReply);
}

void VersionManifestClient::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply) {
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();

    if (!m_fetching) {
        return;
    }

    const bool requestWasMirror = m_usingMirror;
    QString errorReason;

    if (reply->error() != QNetworkReply::NoError) {
        errorReason = reply->errorString();
    } else {
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus >= 400) {
            errorReason = QStringLiteral("服务器返回 HTTP %1").arg(httpStatus);
        } else {
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(reply->readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                errorReason = QStringLiteral("版本清单 JSON 解析失败");
            } else {
                const QJsonObject root = document.object();
                const QJsonObject latest =
                    root.value(QStringLiteral("latest")).toObject();
                MinecraftVersionManifest manifest;
                manifest.latestRelease =
                    latest.value(QStringLiteral("release")).toString().trimmed();
                manifest.latestSnapshot =
                    latest.value(QStringLiteral("snapshot")).toString().trimmed();

                const QJsonArray versions =
                    root.value(QStringLiteral("versions")).toArray();
                manifest.versions.reserve(versions.size());
                for (const QJsonValue &value : versions) {
                    if (!value.isObject()) {
                        continue;
                    }
                    const QJsonObject entry = value.toObject();
                    const QString id =
                        entry.value(QStringLiteral("id")).toString().trimmed();
                    const QString type =
                        entry.value(QStringLiteral("type")).toString().trimmed();
                    const QString url =
                        entry.value(QStringLiteral("url")).toString().trimmed();
                    const QString sha1 =
                        entry.value(QStringLiteral("sha1")).toString().trimmed().toLower();
                    if (id.isEmpty() || url.isEmpty() || sha1.isEmpty()
                        || versionCategory(type).isEmpty()) {
                        continue;
                    }

                    MinecraftVersionEntry version;
                    version.id = id;
                    version.type = type;
                    version.jsonUrl = QUrl(url);
                    version.jsonSha1 = sha1;
                    version.releaseTime = QDateTime::fromString(
                        entry.value(QStringLiteral("releaseTime")).toString(),
                        Qt::ISODate);
                    if (!version.jsonUrl.isValid()) {
                        continue;
                    }
                    manifest.versions.append(version);
                }

                if (manifest.versions.isEmpty()) {
                    errorReason = QStringLiteral("版本清单中没有可用版本");
                } else {
                    m_fetching = false;
                    emit ready(manifest);
                    return;
                }
            }
        }
    }

    if (m_fetching && !m_fallbackTried) {
        m_fallbackTried = true;
        requestManifest(!requestWasMirror);
        return;
    }

    m_fetching = false;
    emit failed(QStringLiteral("获取版本清单失败：%1。").arg(errorReason));
}
