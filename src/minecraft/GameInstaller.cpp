/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "GameInstaller.h"

#include "DownloadManager.h"
#include "DownloadTask.h"
#include "MinecraftPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>

namespace {

const QString kManifestUrl =
    QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

bool isValidVersionId(const QString &versionId)
{
    return !versionId.isEmpty()
        && !versionId.contains(QLatin1Char('/'))
        && !versionId.contains(QLatin1Char('\\'))
        && versionId != QStringLiteral(".")
        && versionId != QStringLiteral("..");
}

} // namespace

GameInstaller::GameInstaller(QString gameDirectory, QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_manager(new DownloadManager(this))
{
    if (gameDirectory.trimmed().isEmpty()) {
        gameDirectory = MinecraftPaths::savedGameDirectory();
    }
    m_gameDirectory = QDir(gameDirectory).absolutePath();

    connect(m_manager, &DownloadManager::overallProgress,
            this, &GameInstaller::overallProgress);
    connect(m_manager, &DownloadManager::overallPercentChanged,
            this, &GameInstaller::overallPercentChanged);
    connect(m_manager, &DownloadManager::speedChanged,
            this, &GameInstaller::speedChanged);
    connect(m_manager, &DownloadManager::taskFailed, this,
            [this](int, const QString &reason) {
                ++m_batchFailures;
                m_lastFailure = reason;
            });
    connect(m_manager, &DownloadManager::finished,
            this, &GameInstaller::handleManagerFinished);
}

QString GameInstaller::gameDirectory() const
{
    return m_gameDirectory;
}

void GameInstaller::setGameDirectory(const QString &directory)
{
    if (m_busy || directory.trimmed().isEmpty()) {
        return;
    }
    m_gameDirectory = QDir(directory).absolutePath();
}

bool GameInstaller::isBusy() const
{
    return m_busy;
}

void GameInstaller::cancel()
{
    if (!m_busy) {
        return;
    }
    m_cancelled = true;

    if (m_manifestReply) {
        QNetworkReply *reply = m_manifestReply.data();
        m_manifestReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    if (m_manager->isRunning()) {
        m_manager->cancel();
        return;
    }

    m_busy = false;
    m_phase = Phase::Idle;
    emit phaseChanged(QStringLiteral("下载已取消"));
    emit cancelled();
}

void GameInstaller::installFromManifest(const QString &versionId,
                                        const QUrl &versionJsonUrl,
                                        const QString &versionJsonSha1)
{
    if (m_busy) {
        return;
    }
    if (!isValidVersionId(versionId)
        || !versionJsonUrl.isValid()
        || (versionJsonUrl.scheme() != QStringLiteral("http")
            && versionJsonUrl.scheme() != QStringLiteral("https"))
        || versionJsonSha1.trimmed().size() != 40) {
        emit failed(QStringLiteral("版本清单中的下载信息不完整。"));
        return;
    }

    m_busy = true;
    m_cancelled = false;
    m_phase = Phase::DownloadingVersionJson;
    m_versionId = versionId;
    m_versionJsonUrl = versionJsonUrl;
    m_versionJsonSha1 = versionJsonSha1.trimmed().toLower().toLatin1();
    m_versionJsonPath.clear();
    resetBatchTracking();

    emit phaseChanged(QStringLiteral("正在获取 Minecraft %1 的版本信息…").arg(versionId));
    emit overallProgress(0, 0);
    emit overallPercentChanged(0);
    downloadVersionJson();
}

void GameInstaller::install(const QString &versionId)
{
    if (m_busy) {
        return;
    }
    if (!isValidVersionId(versionId)) {
        emit failed(QStringLiteral("版本 ID 无效。"));
        return;
    }

    m_busy = true;
    m_cancelled = false;
    m_phase = Phase::FetchingManifest;
    m_versionId = versionId;
    m_versionJsonUrl.clear();
    m_versionJsonSha1.clear();
    m_versionJsonPath.clear();
    resetBatchTracking();

    emit phaseChanged(QStringLiteral("正在获取 Minecraft %1 的版本信息…").arg(versionId));
    emit overallProgress(0, 0);
    emit overallPercentChanged(0);
    fetchManifest();
}

void GameInstaller::fetchManifest()
{
    if (m_manifestReply) {
        disconnect(m_manifestReply.data(), nullptr, this, nullptr);
        m_manifestReply->abort();
        m_manifestReply->deleteLater();
        m_manifestReply = nullptr;
    }

    QNetworkRequest request{QUrl(kManifestUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MysticTurtle/2.0"));
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_network->get(request);
    if (!reply) {
        fail(QStringLiteral("无法发起版本清单请求。"));
        return;
    }

    m_manifestReply = reply;
    connect(reply, &QNetworkReply::finished, this, &GameInstaller::handleManifestReply);
}

void GameInstaller::handleManifestReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_manifestReply) {
        return;
    }
    m_manifestReply = nullptr;
    reply->deleteLater();

    if (!m_busy || m_phase != Phase::FetchingManifest) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("获取版本清单失败：%1").arg(reply->errorString()));
        return;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 400) {
        fail(QStringLiteral("获取版本清单失败：服务器返回 HTTP %1。").arg(httpStatus));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("版本清单 JSON 解析失败。"));
        return;
    }

    const QJsonArray versions =
        document.object().value(QStringLiteral("versions")).toArray();
    for (const QJsonValue &value : versions) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() != m_versionId) {
            continue;
        }

        m_versionJsonUrl =
            QUrl(entry.value(QStringLiteral("url")).toString().trimmed());
        m_versionJsonSha1 =
            entry.value(QStringLiteral("sha1")).toString().trimmed().toLatin1();
        if (!m_versionJsonUrl.isValid() || m_versionJsonSha1.isEmpty()) {
            fail(QStringLiteral("版本清单中的版本信息不完整。"));
            return;
        }

        downloadVersionJson();
        return;
    }

    fail(QStringLiteral("版本清单中找不到 Minecraft %1。").arg(m_versionId));
}

void GameInstaller::downloadVersionJson()
{
    m_phase = Phase::DownloadingVersionJson;
    m_versionJsonPath = versionJsonPath();
    resetBatchTracking();

    emit phaseChanged(QStringLiteral("正在下载 Minecraft %1 版本信息…").arg(m_versionId));

    QList<DownloadTask *> tasks;
    tasks.append(new DownloadTask(m_versionJsonUrl, m_versionJsonPath, m_versionJsonSha1));
    if (!m_manager->start(tasks)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("无法启动版本信息下载。"));
    }
}

void GameInstaller::downloadGameFiles(const VersionParser::VersionInfo &version)
{
    m_phase = Phase::DownloadingFiles;
    resetBatchTracking();

    QList<DownloadTask *> tasks;
    QSet<QString> targetPaths;

    const auto addDownload = [&](const VersionParser::DownloadInfo &info,
                                 bool requireSha1) -> bool {
        if (info.url.isEmpty() || info.relativePath.isEmpty()) {
            return false;
        }
        if (requireSha1 && info.sha1.size() != 40) {
            return false;
        }
        if (!info.sha1.isEmpty() && info.sha1.size() != 40) {
            return false;
        }

        const QString absolutePath = QDir(m_gameDirectory).filePath(info.relativePath);
        if (targetPaths.contains(absolutePath)) {
            return true;
        }
        targetPaths.insert(absolutePath);
        tasks.append(new DownloadTask(QUrl(info.url), absolutePath,
                                      info.sha1.toLatin1()));
        return true;
    };

    if (!addDownload(version.client, false)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("版本 JSON 缺少客户端下载信息。"));
        return;
    }

    if (!version.assetIndex.url.isEmpty()
        && !addDownload(version.assetIndex, false)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("版本 JSON 的资源索引下载信息不完整。"));
        return;
    }
    m_assetIndexPath = version.assetIndex.relativePath.isEmpty()
        ? QString()
        : QDir(m_gameDirectory).filePath(version.assetIndex.relativePath);

    for (const VersionParser::Library &library : version.libraries) {
        if (!addDownload(library.download, false)) {
            for (DownloadTask *task : tasks) {
                delete task;
            }
            fail(QStringLiteral("依赖库 %1 的下载信息不完整。").arg(library.name));
            return;
        }
    }

    if (tasks.isEmpty()) {
        finishSuccess();
        return;
    }

    emit phaseChanged(QStringLiteral("正在下载 %1 个游戏文件…").arg(tasks.size()));
    if (!m_manager->start(tasks)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("无法启动游戏文件下载。"));
    }
}

void GameInstaller::downloadAssets()
{
    if (m_assetIndexPath.isEmpty() || !QFileInfo::exists(m_assetIndexPath)) {
        finishSuccess();
        return;
    }

    QFile indexFile(m_assetIndexPath);
    if (!indexFile.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("无法读取资源索引：%1").arg(m_assetIndexPath));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(indexFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("资源索引 JSON 解析失败。"));
        return;
    }

    const QJsonObject objects = document.object()
                                    .value(QStringLiteral("objects")).toObject();
    if (objects.isEmpty()) {
        finishSuccess();
        return;
    }

    m_phase = Phase::DownloadingAssets;
    resetBatchTracking();

    QList<DownloadTask *> tasks;
    QSet<QString> targetPaths;
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QJsonObject object = it.value().toObject();
        const QString hash = object.value(QStringLiteral("hash")).toString().trimmed();
        const qint64 size = static_cast<qint64>(
            object.value(QStringLiteral("size")).toDouble());
        if (hash.size() != 40) {
            continue;
        }

        const QString relativePath =
            QStringLiteral("assets/objects/%1/%2").arg(hash.left(2), hash);
        const QString absolutePath = QDir(m_gameDirectory).filePath(relativePath);
        if (targetPaths.contains(absolutePath)) {
            continue;
        }
        if (QFileInfo::exists(absolutePath)
            && (size <= 0 || QFileInfo(absolutePath).size() == size)) {
            continue;
        }
        targetPaths.insert(absolutePath);
        tasks.append(new DownloadTask(
            QUrl(QStringLiteral("https://resources.download.minecraft.net/%1/%2")
                     .arg(hash.left(2), hash)),
            absolutePath, hash.toLatin1()));
    }

    if (tasks.isEmpty()) {
        finishSuccess();
        return;
    }

    emit phaseChanged(QStringLiteral("正在下载 %1 个资源文件…").arg(tasks.size()));
    if (!m_manager->start(tasks)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("无法启动资源文件下载。"));
    }
}

void GameInstaller::handleManagerFinished()
{
    if (m_cancelled) {
        m_busy = false;
        m_phase = Phase::Idle;
        emit phaseChanged(QStringLiteral("下载已取消"));
        emit cancelled();
        return;
    }
    if (!m_busy) {
        return;
    }

    if (m_phase == Phase::DownloadingVersionJson) {
        const int failures = m_batchFailures;
        const QString lastFailure = m_lastFailure;

        // The manager is still emitting finished(). Defer starting the next
        // batch so it can finish deleting the previous batch's task objects.
        QTimer::singleShot(0, this, [this, failures, lastFailure]() {
            if (!m_busy) {
                return;
            }
            if (failures > 0) {
                fail(QStringLiteral("版本信息下载失败：%1").arg(lastFailure));
                return;
            }

            VersionParser parser;
            QString error;
            if (!parser.parseFile(m_versionJsonPath, &error)) {
                fail(QStringLiteral("版本 JSON 解析失败：%1").arg(error));
                return;
            }

            downloadGameFiles(parser.version());
        });
        return;
    }

    if (m_phase == Phase::DownloadingFiles) {
        if (m_batchFailures > 0) {
            fail(QStringLiteral("有 %1 个文件下载失败：%2")
                     .arg(m_batchFailures)
                     .arg(m_lastFailure));
            return;
        }
        QTimer::singleShot(0, this, [this]() {
            if (m_busy) {
                downloadAssets();
            }
        });
        return;
    }

    if (m_phase == Phase::DownloadingAssets) {
        if (m_batchFailures > 0) {
            fail(QStringLiteral("有 %1 个资源文件下载失败：%2")
                     .arg(m_batchFailures)
                     .arg(m_lastFailure));
            return;
        }
        finishSuccess();
    }
}

void GameInstaller::fail(const QString &reason)
{
    m_busy = false;
    m_phase = Phase::Idle;

    if (m_manifestReply) {
        QNetworkReply *reply = m_manifestReply.data();
        m_manifestReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }

    emit phaseChanged(QStringLiteral("下载失败"));
    emit failed(reason);
}

void GameInstaller::finishSuccess()
{
    m_busy = false;
    m_phase = Phase::Idle;

    emit overallPercentChanged(100);
    emit phaseChanged(QStringLiteral("下载完成"));
    emit succeeded(m_versionId);
}

void GameInstaller::resetBatchTracking()
{
    m_batchFailures = 0;
    m_lastFailure.clear();
}

QString GameInstaller::versionJsonPath() const
{
    return QDir(m_gameDirectory).filePath(
        QStringLiteral("versions/%1/%1.json").arg(m_versionId));
}
