/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "LoaderDownloader.h"

#include "DownloadManager.h"
#include "DownloadTask.h"
#include "GameLauncher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUuid>

namespace {

const QString kFabricMaven = QStringLiteral("https://maven.fabricmc.net/");

bool isValidPathSegment(const QString &value)
{
    return !value.isEmpty()
        && !value.contains(QLatin1Char('/'))
        && !value.contains(QLatin1Char('\\'))
        && value != QStringLiteral(".")
        && value != QStringLiteral("..");
}

QString sanitizedFileName(const QString &fileName, const QString &fallback)
{
    QString name = QFileInfo(fileName).fileName().trimmed();
    if (name.isEmpty()) {
        name = fallback;
    }
    return name;
}

QString consoleJava(const QString &java)
{
    if (java.isEmpty()) {
        return java;
    }
    const QFileInfo info(java);
    if (info.fileName().compare(QStringLiteral("javaw.exe"), Qt::CaseInsensitive) == 0) {
        const QString sibling = info.absoluteDir().filePath(QStringLiteral("java.exe"));
        if (QFileInfo::exists(sibling)) {
            return sibling;
        }
    }
    return java;
}

bool createJunction(const QString &linkPath, const QString &targetPath, QString *errorMessage)
{
    QProcess process;
    process.setProgram(QStringLiteral("cmd.exe"));
    process.setArguments(QStringList{
        QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
        QDir::toNativeSeparators(linkPath), QDir::toNativeSeparators(targetPath)});
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(10000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = QStringLiteral("创建安装目录链接超时。");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建安装目录链接：%1")
                                .arg(QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed());
        }
        return false;
    }
    return QFileInfo(linkPath).isDir();
}

void removeJunction(const QString &linkPath)
{
    QProcess process;
    process.setProgram(QStringLiteral("cmd.exe"));
    process.setArguments(QStringList{
        QStringLiteral("/c"), QStringLiteral("rmdir"),
        QDir::toNativeSeparators(linkPath)});
    process.start();
    if (!process.waitForStarted(3000) || !process.waitForFinished(5000)) {
        process.kill();
    }
}

} // namespace

LoaderDownloader::LoaderDownloader(const QString &gameDirectory, QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_manager(new DownloadManager(this))
{
    m_gameDirectory = QDir(gameDirectory).absolutePath();

    connect(m_manager, &DownloadManager::overallPercentChanged,
            this, &LoaderDownloader::percentChanged);
    connect(m_manager, &DownloadManager::speedChanged,
            this, &LoaderDownloader::speedChanged);
    connect(m_manager, &DownloadManager::taskFailed, this,
            [this](int, const QString &reason) {
                ++m_failures;
                m_lastFailure = reason;
            });
    connect(m_manager, &DownloadManager::finished,
            this, &LoaderDownloader::handleDownloadsFinished);
}

void LoaderDownloader::start(const QString &baseVersion, const LoaderSelection &selection)
{
    if (m_busy || !selection.hasLoader()) {
        return;
    }

    m_busy = true;
    m_baseVersion = baseVersion;
    m_selection = selection;
    m_fabricVersionId.clear();
    m_installerPath.clear();
    m_installerLog.clear();
    m_failures = 0;
    m_lastFailure.clear();

    if (m_selection.loader == QStringLiteral("Fabric")) {
        fetchFabricProfile();
    } else {
        downloadInstaller();
    }
}

bool LoaderDownloader::isBusy() const
{
    return m_busy;
}

void LoaderDownloader::fetchFabricProfile()
{
    if (m_profileReply) {
        disconnect(m_profileReply.data(), nullptr, this, nullptr);
        m_profileReply->abort();
        m_profileReply->deleteLater();
        m_profileReply = nullptr;
    }

    const QString urlString =
        QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
            .arg(m_baseVersion, m_selection.loaderVersion);
    QNetworkRequest request{QUrl(urlString)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MysticTurtle/0.1"));
    request.setTransferTimeout(30000);

    emit phaseChanged(QStringLiteral("正在获取 Fabric 安装配置…"));
    emit percentChanged(0);

    QNetworkReply *reply = m_network->get(request);
    if (!reply) {
        fail(QStringLiteral("无法请求 Fabric 安装配置。"));
        return;
    }

    m_profileReply = reply;
    connect(reply, &QNetworkReply::finished, this, &LoaderDownloader::handleFabricProfile);
}

void LoaderDownloader::handleFabricProfile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_profileReply) {
        return;
    }
    m_profileReply = nullptr;
    reply->deleteLater();

    if (!m_busy) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("获取 Fabric 安装配置失败：%1").arg(reply->errorString()));
        return;
    }
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 400) {
        fail(QStringLiteral("获取 Fabric 安装配置失败：HTTP %1。").arg(httpStatus));
        return;
    }

    const QByteArray profileJson = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(profileJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("Fabric 安装配置 JSON 解析失败。"));
        return;
    }

    startFabricDownloads(profileJson);
}

void LoaderDownloader::startFabricDownloads(const QByteArray &profileJson)
{
    const QJsonObject root = QJsonDocument::fromJson(profileJson).object();
    QString versionId = root.value(QStringLiteral("id")).toString().trimmed();
    if (!isValidPathSegment(versionId)) {
        versionId = QStringLiteral("fabric-loader-%1-%2")
                        .arg(m_selection.loaderVersion, m_baseVersion);
        versionId.replace(QLatin1Char('/'), QLatin1Char('-'));
        versionId.replace(QLatin1Char('\\'), QLatin1Char('-'));
    }
    m_fabricVersionId = versionId;

    const QString versionDirectory =
        QDir(m_gameDirectory).filePath(QStringLiteral("versions/%1").arg(versionId));
    if (!QDir().mkpath(versionDirectory)) {
        fail(QStringLiteral("无法创建 Fabric 版本目录。"));
        return;
    }

    const QString versionJsonPath =
        QDir(versionDirectory).filePath(QStringLiteral("%1.json").arg(versionId));
    QFile versionFile(versionJsonPath);
    if (!versionFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || versionFile.write(profileJson) != profileJson.size()) {
        fail(QStringLiteral("无法保存 Fabric 版本 JSON。"));
        return;
    }
    versionFile.close();

    QList<DownloadTask *> tasks;
    QSet<QString> targetPaths;

    const QJsonArray libraries = root.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &value : libraries) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject library = value.toObject();
        const QString name = library.value(QStringLiteral("name")).toString().trimmed();
        const QString mavenPath = mavenRelativePath(name);
        if (mavenPath.isEmpty()) {
            continue;
        }

        QString baseUrl = library.value(QStringLiteral("url")).toString().trimmed();
        if (baseUrl.isEmpty()) {
            baseUrl = kFabricMaven;
        }
        if (!baseUrl.endsWith(QLatin1Char('/'))) {
            baseUrl += QLatin1Char('/');
        }

        const QString relativePath = QStringLiteral("libraries/%1").arg(mavenPath);
        const QString absolutePath = QDir(m_gameDirectory).filePath(relativePath);
        if (targetPaths.contains(absolutePath)) {
            continue;
        }
        targetPaths.insert(absolutePath);
        tasks.append(new DownloadTask(
            QUrl(baseUrl + mavenPath), absolutePath,
            library.value(QStringLiteral("sha1")).toString().trimmed().toLatin1()));
    }

    if (!m_selection.fabricApiUrl.isEmpty()) {
        const QString apiFileName = sanitizedFileName(
            m_selection.fabricApiFilename,
            QStringLiteral("fabric-api-%1.jar").arg(m_selection.fabricApiVersion));
        const QString apiPath =
            QDir(m_gameDirectory).filePath(QStringLiteral("mods/%1").arg(apiFileName));
        if (!targetPaths.contains(apiPath)) {
            targetPaths.insert(apiPath);
            tasks.append(new DownloadTask(
                QUrl(m_selection.fabricApiUrl), apiPath,
                m_selection.fabricApiSha1.toLatin1()));
        }
    }

    if (tasks.isEmpty()) {
        fail(QStringLiteral("Fabric 安装配置中没有可下载的文件。"));
        return;
    }

    m_failures = 0;
    m_lastFailure.clear();
    emit phaseChanged(QStringLiteral("正在下载 Fabric 组件与 Fabric API…"));
    emit percentChanged(0);
    if (!m_manager->start(tasks)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("无法启动 Fabric 文件下载。"));
    }
}

void LoaderDownloader::downloadInstaller()
{
    if (m_selection.downloadUrl.isEmpty()) {
        fail(QStringLiteral("安装器下载地址为空。"));
        return;
    }

    const QString fallbackName =
        QStringLiteral("%1-%2-installer.jar")
            .arg(m_selection.loader, m_selection.loaderVersion);
    const QString fileName =
        sanitizedFileName(m_selection.downloadFilename, fallbackName);
    const QString installerPath = QDir(m_gameDirectory)
                                      .filePath(QStringLiteral("installers/%1").arg(fileName));
    m_installerPath = installerPath;

    emit phaseChanged(
        QStringLiteral("正在下载 %1 %2 安装器…")
            .arg(m_selection.loader, m_selection.loaderVersion));
    emit percentChanged(0);

    QList<DownloadTask *> tasks;
    tasks.append(new DownloadTask(QUrl(m_selection.downloadUrl), installerPath,
                                  m_selection.downloadSha1.toLatin1()));
    m_failures = 0;
    m_lastFailure.clear();
    if (!m_manager->start(tasks)) {
        for (DownloadTask *task : tasks) {
            delete task;
        }
        fail(QStringLiteral("无法启动安装器下载。"));
    }
}

void LoaderDownloader::handleDownloadsFinished()
{
    if (!m_busy) {
        return;
    }

    if (m_failures > 0) {
        fail(QStringLiteral("有 %1 个文件下载失败：%2")
                 .arg(m_failures)
                 .arg(m_lastFailure));
        return;
    }

    m_busy = false;
    emit percentChanged(100);

    QString summary;
    if (m_selection.loader == QStringLiteral("Fabric")) {
        summary = QStringLiteral("Fabric %1 与 Fabric API %2 下载完成。")
                      .arg(m_selection.loaderVersion, m_selection.fabricApiVersion);
        emit phaseChanged(QStringLiteral("Fabric 组件下载完成"));
    } else if (m_selection.loader == QStringLiteral("OptiFine")) {
        summary = QStringLiteral("%1 %2 安装器已下载至 installers 目录；"
                                 "官方安装器暂不支持静默安装，请手动运行。")
                      .arg(m_selection.loader, m_selection.loaderVersion);
        emit phaseChanged(QStringLiteral("OptiFine 安装器下载完成"));
    } else {
        // Forge / NeoForge installer download completed; run it now.
        m_busy = true;
        startInstaller();
        return;
    }
    emit finished(summary);
}

void LoaderDownloader::startInstaller()
{
    if (!m_busy || m_installerPath.isEmpty() || !QFileInfo::exists(m_installerPath)) {
        fail(QStringLiteral("安装器文件不存在或已损坏。"));
        return;
    }

    m_busy = true;
    m_installerLog.clear();
    const int requiredJava =
        GameLauncher::requiredJavaMajor(m_gameDirectory, m_baseVersion);
    QString compatibleJava = GameLauncher::findCompatibleJava(
        m_gameDirectory, requiredJava, nullptr);
    if (compatibleJava.isEmpty()) {
        const QSettings settings;
        const QString configured =
            settings.value(QStringLiteral("game/javaPath")).toString().trimmed();
        if (!configured.isEmpty() && QFileInfo::exists(configured)) {
            compatibleJava = configured;
        }
    }
    const QString java = compatibleJava.isEmpty()
        ? consoleJava(GameLauncher::findJavaExecutable(m_gameDirectory))
        : consoleJava(compatibleJava);
    if (java.isEmpty()) {
        fail(QStringLiteral("未找到 Java，无法运行安装器。"));
        return;
    }

    m_installerTempHome = QDir(QDir::tempPath()).filePath(
        QStringLiteral("MysticTurtle-install-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(m_installerTempHome)) {
        fail(QStringLiteral("无法创建安装器临时目录。"));
        return;
    }

    const QString linkPath = QDir(m_installerTempHome).filePath(QStringLiteral(".minecraft"));
    QString linkError;
    if (!createJunction(linkPath, m_gameDirectory, &linkError)) {
        cleanupInstallerTemp();
        fail(linkError);
        return;
    }

    const QString profilesPath =
        QDir(m_gameDirectory).filePath(QStringLiteral("launcher_profiles.json"));
    if (!QFileInfo::exists(profilesPath)) {
        QJsonObject root;
        root.insert(QStringLiteral("profiles"), QJsonObject());
        QJsonObject settings;
        settings.insert(QStringLiteral("enableSnapshots"), false);
        settings.insert(QStringLiteral("keepLauncherOpen"), false);
        root.insert(QStringLiteral("settings"), settings);
        root.insert(QStringLiteral("version"), 3);
        QFile profileFile(profilesPath);
        if (profileFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            profileFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }
    }

    QStringList arguments;
    arguments.append(QStringLiteral("-Duser.home=%1")
                         .arg(QDir::toNativeSeparators(m_installerTempHome)));
    arguments.append(QStringLiteral("-Dminecraft.launcher.brand=MysticTurtle"));
    arguments.append(QStringLiteral("-jar"));
    arguments.append(QDir::toNativeSeparators(m_installerPath));
    if (m_selection.loader == QStringLiteral("NeoForge")) {
        arguments.append(QStringLiteral("--install-client"));
    } else {
        arguments.append(QStringLiteral("--installClient"));
    }

    m_installerProcess = new QProcess(this);
    m_installerProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_installerProcess->setWorkingDirectory(m_gameDirectory);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("APPDATA"), QDir::toNativeSeparators(m_installerTempHome));
    environment.insert(QStringLiteral("HOME"), QDir::toNativeSeparators(m_installerTempHome));
    environment.insert(QStringLiteral("USERPROFILE"),
                       QDir::toNativeSeparators(m_installerTempHome));
    m_installerProcess->setProcessEnvironment(environment);

    connect(m_installerProcess, &QProcess::readyRead, this,
            &LoaderDownloader::handleInstallerOutput);
    connect(m_installerProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &LoaderDownloader::handleInstallerFinished);
    connect(m_installerProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;
                }
                const QString detail = m_installerProcess
                    ? m_installerProcess->errorString()
                    : QStringLiteral("未知错误");
                cleanupInstallerTemp();
                fail(QStringLiteral("无法启动安装器：%1").arg(detail));
            });

    m_installerTimeout = new QTimer(this);
    m_installerTimeout->setSingleShot(true);
    connect(m_installerTimeout, &QTimer::timeout, this, [this]() {
        if (!m_installerProcess) {
            return;
        }
        m_installerLog += QStringLiteral("\n安装器运行超过 15 分钟，已终止。\n");
        m_installerProcess->kill();
    });

    emit phaseChanged(
        QStringLiteral("正在自动安装 %1 %2…")
            .arg(m_selection.loader, m_selection.loaderVersion));
    emit percentChanged(100);
    emit speedChanged(0);

    m_installerProcess->start(java, arguments);
    if (!m_installerProcess->waitForStarted(8000)) {
        cleanupInstallerTemp();
        fail(QStringLiteral("Java 安装器启动失败：%1")
                 .arg(m_installerProcess ? m_installerProcess->errorString()
                                         : QStringLiteral("未知错误")));
        return;
    }
    if (!m_installerProcess || !m_installerTimeout) {
        return;
    }
    m_installerTimeout->start(15 * 60 * 1000);
}

void LoaderDownloader::handleInstallerOutput()
{
    if (!m_installerProcess) {
        return;
    }
    m_installerLog += QString::fromLocal8Bit(m_installerProcess->readAll());
    if (m_installerLog.size() > 24000) {
        m_installerLog = m_installerLog.right(24000);
    }
}

void LoaderDownloader::handleInstallerFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_installerTimeout) {
        m_installerTimeout->stop();
        m_installerTimeout->deleteLater();
        m_installerTimeout = nullptr;
    }

    QProcess *process = m_installerProcess.data();
    if (process) {
        m_installerLog += QString::fromLocal8Bit(process->readAll());
        if (m_installerLog.size() > 24000) {
            m_installerLog = m_installerLog.right(24000);
        }
    }
    m_installerProcess = nullptr;
    if (process) {
        process->deleteLater();
    }

    if (!m_busy) {
        cleanupInstallerTemp();
        return;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        cleanupInstallerTemp();
        fail(QStringLiteral("%1 自动安装失败（退出码 %2）：%3")
                 .arg(m_selection.loader)
                 .arg(exitCode)
                 .arg(installerLogTail()));
        return;
    }

    const QString installedVersion = findInstalledVersionId();
    cleanupInstallerTemp();
    if (installedVersion.isEmpty()) {
        fail(QStringLiteral("%1 安装器结束，但没有找到新的版本配置。%2")
                 .arg(m_selection.loader, installerLogTail()));
        return;
    }

    m_busy = false;
    emit percentChanged(100);
    emit phaseChanged(QStringLiteral("%1 安装完成").arg(m_selection.loader));
    emit finished(QStringLiteral("%1 %2 已自动安装，版本：%3")
                      .arg(m_selection.loader, m_selection.loaderVersion, installedVersion));
}

QString LoaderDownloader::installerLogTail() const
{
    QString log = m_installerLog;
    log.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    log = log.trimmed();
    if (log.size() > 1600) {
        log = log.right(1600);
    }
    return log.isEmpty() ? QStringLiteral("安装器没有输出日志。") : log;
}

QString LoaderDownloader::findInstalledVersionId() const
{
    const QDir versions(QDir(m_gameDirectory).filePath(QStringLiteral("versions")));
    const QStringList exactIds = {
        QStringLiteral("%1-%2-%3").arg(
            m_baseVersion,
            m_selection.loader == QStringLiteral("NeoForge")
                ? QStringLiteral("neoforge")
                : QStringLiteral("forge"),
            m_selection.loaderVersion),
        QStringLiteral("%1-%2").arg(
            m_selection.loader == QStringLiteral("NeoForge")
                ? QStringLiteral("neoforge")
                : QStringLiteral("forge"),
            m_selection.loaderVersion)
    };

    for (const QString &id : exactIds) {
        const QString jsonPath =
            QDir(versions.filePath(id)).filePath(QStringLiteral("%1.json").arg(id));
        if (QFileInfo::exists(jsonPath)) {
            return id;
        }
    }

    QString bestId;
    QDateTime bestTime;
    const QFileInfoList entries = versions.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo &entry : entries) {
        const QDir versionDir(entry.absoluteFilePath());
        const QFileInfoList jsonFiles = versionDir.entryInfoList(
            QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Time);
        for (const QFileInfo &jsonFile : jsonFiles) {
            QFile file(jsonFile.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            if (!document.isObject()) {
                continue;
            }
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("inheritsFrom")).toString() != m_baseVersion) {
                continue;
            }
            const QString id = object.value(QStringLiteral("id")).toString().trimmed();
            const QString lower = (id + QLatin1Char(' ') + entry.fileName()).toLower();
            const bool matchesLoader = m_selection.loader == QStringLiteral("NeoForge")
                ? lower.contains(QStringLiteral("neoforge"))
                : lower.contains(QStringLiteral("forge"));
            if (!matchesLoader) {
                continue;
            }
            if (bestId.isEmpty() || jsonFile.lastModified() > bestTime) {
                bestId = entry.fileName();
                bestTime = jsonFile.lastModified();
            }
        }
    }
    return bestId;
}

void LoaderDownloader::cleanupInstallerTemp()
{
    if (m_installerTempHome.isEmpty()) {
        return;
    }

    const QString tempHome = m_installerTempHome;
    const QString linkPath = QDir(tempHome).filePath(QStringLiteral(".minecraft"));
    if (QFileInfo::exists(linkPath) || QFileInfo(linkPath).isSymLink()) {
        removeJunction(linkPath);
    }

    const bool linkStillExists = QFileInfo::exists(linkPath)
        || QFileInfo(linkPath).isSymLink();
    if (!linkStillExists) {
        QDir(tempHome).removeRecursively();
    }
    m_installerTempHome.clear();
}

void LoaderDownloader::fail(const QString &reason)
{
    if (!m_busy) {
        return;
    }
    m_busy = false;

    if (m_installerTimeout) {
        m_installerTimeout->stop();
        m_installerTimeout->deleteLater();
        m_installerTimeout = nullptr;
    }

    if (m_installerProcess) {
        m_installerProcess->kill();
        m_installerProcess->waitForFinished(5000);
        m_installerProcess->deleteLater();
        m_installerProcess = nullptr;
    }

    cleanupInstallerTemp();

    if (m_profileReply) {
        QNetworkReply *reply = m_profileReply.data();
        m_profileReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    emit phaseChanged(QStringLiteral("组件下载失败"));
    emit failed(reason);
}

QString LoaderDownloader::mavenRelativePath(const QString &coordinates) const
{
    QString coordinate = coordinates.trimmed();
    QString extension = QStringLiteral("jar");
    const qsizetype extensionAt = coordinate.lastIndexOf(QLatin1Char('@'));
    if (extensionAt >= 0) {
        extension = coordinate.mid(extensionAt + 1);
        coordinate.truncate(extensionAt);
    }

    const QStringList parts = coordinate.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }

    QString groupPath = parts.at(0);
    groupPath.replace(QLatin1Char('.'), QLatin1Char('/'));
    QString fileName = QStringLiteral("%1-%2").arg(parts.at(1), parts.at(2));
    if (parts.size() >= 4 && !parts.at(3).isEmpty()) {
        fileName += QLatin1Char('-') + parts.at(3);
    }
    fileName += QLatin1Char('.') + (extension.isEmpty() ? QStringLiteral("jar") : extension);
    return QStringLiteral("%1/%2/%3/%4")
        .arg(groupPath, parts.at(1), parts.at(2), fileName);
}
