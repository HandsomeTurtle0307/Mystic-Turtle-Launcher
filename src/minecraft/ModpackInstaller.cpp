/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include "ModpackInstaller.h"

#include "GameInstaller.h"
#include "LoaderDownloader.h"
#include "DownloadManager.h"
#include "DownloadTask.h"
#include "settings/GlobalSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool validVersionPart(const QString &value)
{
    static const QRegularExpression allowed(
        QStringLiteral("^[A-Za-z0-9._+-]{1,80}$"));
    return allowed.match(value).hasMatch() && value != QStringLiteral(".")
        && value != QStringLiteral("..");
}

QString safeRelativePath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char(':'))) {
        return QString();
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QString();
    }
    for (const QString &part : parts) {
        if (part == QStringLiteral(".") || part == QStringLiteral("..")) {
            return QString();
        }
    }
    return parts.join(QLatin1Char('/'));
}

QString safeInstanceName(QString name)
{
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]")),
                 QStringLiteral("-"));
    name.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
    name = name.trimmed();
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        name.chop(1);
    }
    if (name.isEmpty() || name == QStringLiteral(".")
        || name == QStringLiteral("..")) {
        name = QStringLiteral("Modpack");
    }
    if (name.size() > 48) {
        name = name.left(48);
    }
    return name;
}

QString jsonVersionPath(const QString &gameDirectory, const QString &versionId)
{
    return QDir(gameDirectory).filePath(
        QStringLiteral("versions/%1/%1.json").arg(versionId));
}

bool validSha(const QString &hash, int length)
{
    static const QRegularExpression hexadecimal(QStringLiteral("^[0-9a-fA-F]+$"));
    return hash.size() == length && hexadecimal.match(hash).hasMatch();
}

} // namespace

ModpackInstaller::ModpackInstaller(QString gameDirectory, QObject *parent)
    : QObject(parent)
    , m_gameDirectory(QDir(std::move(gameDirectory)).absolutePath())
    , m_manager(new DownloadManager(this))
{
    connect(m_manager, &DownloadManager::taskStarted, this,
            [this](int index) {
        emit modProgressChanged(index + 1, m_manager->totalCount());
        emit phaseChanged(QStringLiteral("正在下载Mod (%1/%2)")
                              .arg(index + 1)
                              .arg(m_manager->totalCount()));
    });
    connect(m_manager, &DownloadManager::taskFailed, this,
            [this](int, const QString &reason) {
        ++m_downloadFailures;
        m_lastFailure = reason;
    });
    connect(m_manager, &DownloadManager::finished,
            this, &ModpackInstaller::downloadModFiles);
}

void ModpackInstaller::start(const QUrl &url, const QString &fileName,
                             const QString &sha1, const QString &displayName)
{
    if (m_busy) {
        return;
    }
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))) {
        emit failed(QStringLiteral("整合包下载链接无效。"));
        return;
    }

    reset();
    m_busy = true;
    m_displayName = displayName.trimmed().isEmpty()
        ? QFileInfo(fileName).completeBaseName()
        : displayName.trimmed();
    m_temporaryDirectory = new QTemporaryDir(
        QDir(QDir::tempPath()).filePath(QStringLiteral("MysticTurtle-modpack-XXXXXX")));
    if (!m_temporaryDirectory->isValid()) {
        fail(QStringLiteral("无法创建整合包临时目录。"));
        return;
    }

    const QString archivePath =
        QDir(m_temporaryDirectory->path()).filePath(QStringLiteral("pack.mrpack"));
    m_phase = Phase::DownloadingPack;
    emit phaseChanged(QStringLiteral("正在下载整合包"));
    emit percentChanged(0);

    m_packTask = new DownloadTask(url, archivePath, sha1.toLatin1(), this);
    connect(m_packTask, &DownloadTask::succeeded,
            this, &ModpackInstaller::handlePackDownloaded);
    connect(m_packTask, &DownloadTask::failed, this,
            [this](const QString &reason) {
        if (m_cancelled) {
            finishCancelled();
        } else {
            fail(QStringLiteral("整合包下载失败：%1").arg(reason));
        }
    });
    connect(m_packTask, &DownloadTask::finished, m_packTask, &QObject::deleteLater);
    m_packTask->start();
}

bool ModpackInstaller::isBusy() const
{
    return m_busy;
}

void ModpackInstaller::cancel()
{
    if (!m_busy || m_cancelled) {
        return;
    }
    m_cancelled = true;
    if (m_packTask && !m_packTask->isFinished()) {
        m_packTask->cancel();
        return;
    }
    if (m_gameInstaller && m_gameInstaller->isBusy()) {
        m_gameInstaller->cancel();
        return;
    }
    if (m_loaderDownloader && m_loaderDownloader->isBusy()) {
        m_loaderDownloader->cancel();
        return;
    }
    if (m_manager->isRunning()) {
        m_manager->cancel();
        return;
    }
    if (m_extractProcess && m_extractProcess->state() != QProcess::NotRunning) {
        m_extractProcess->kill();
        return;
    }
    finishCancelled();
}

void ModpackInstaller::handlePackDownloaded(const QString &path)
{
    if (m_cancelled) {
        finishCancelled();
        return;
    }
    m_phase = Phase::Extracting;
    emit phaseChanged(QStringLiteral("正在解析整合包"));
    emit percentChanged(2);

    const QString extractionPath =
        QDir(m_temporaryDirectory->path()).filePath(QStringLiteral("extracted"));
    if (!QDir().mkpath(extractionPath)) {
        fail(QStringLiteral("无法创建整合包解压目录。"));
        return;
    }

    m_extractProcess = new QProcess(this);
    connect(m_extractProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ModpackInstaller::handleExtractFinished);
    connect(m_extractProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("无法启动整合包解压程序：%1")
                     .arg(m_extractProcess ? m_extractProcess->errorString()
                                           : QStringLiteral("未知错误")));
        }
    });
#ifdef Q_OS_WIN
    const QString script = QStringLiteral(
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$archive=[IO.Compression.ZipFile]::OpenRead($env:MYSTIC_MRPACK_ARCHIVE); "
        "$root=[IO.Path]::GetFullPath($env:MYSTIC_MRPACK_DEST).TrimEnd('\\')+'\\'; "
        "try { foreach($entry in $archive.Entries) { "
        "$name=$entry.FullName.Replace('/','\\'); "
        "if([string]::IsNullOrWhiteSpace($name)){continue}; "
        "if((($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000)"
        "{throw 'Symbolic links are not allowed in modpacks'}; "
        "$target=[IO.Path]::GetFullPath([IO.Path]::Combine($root,$name)); "
        "if(-not $target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase))"
        "{throw 'Unsafe path in modpack archive'}; "
        "if($entry.FullName.EndsWith('/')){[IO.Directory]::CreateDirectory($target)|Out-Null}"
        "else{$parent=[IO.Path]::GetDirectoryName($target);"
        "[IO.Directory]::CreateDirectory($parent)|Out-Null;"
        "[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)}"
        "} } finally {$archive.Dispose()}");
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("MYSTIC_MRPACK_ARCHIVE"), path);
    environment.insert(QStringLiteral("MYSTIC_MRPACK_DEST"), extractionPath);
    m_extractProcess->setProcessEnvironment(environment);
    m_extractProcess->setProgram(QStringLiteral("powershell.exe"));
    m_extractProcess->setArguments({
        QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-Command"), script
    });
#else
    m_extractProcess->setProgram(QStringLiteral("unzip"));
    m_extractProcess->setArguments(
        {QStringLiteral("-o"), path, QStringLiteral("-d"), extractionPath});
#endif
    m_extractProcess->start();
}

void ModpackInstaller::handleExtractFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_busy) {
        return;
    }
    if (m_extractProcess) {
        const QString output = QString::fromLocal8Bit(m_extractProcess->readAllStandardError()
                                                       + m_extractProcess->readAllStandardOutput())
                                   .trimmed();
        m_extractProcess->deleteLater();
        m_extractProcess = nullptr;
        if (m_cancelled) {
            finishCancelled();
            return;
        }
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(QStringLiteral("整合包解压失败：%1")
                     .arg(output.isEmpty() ? QStringLiteral("解压程序异常退出。")
                                           : output));
            return;
        }
    }
    parseIndexAndInstall();
}

void ModpackInstaller::parseIndexAndInstall()
{
    m_indexPath = QDir(m_temporaryDirectory->path())
                      .filePath(QStringLiteral("extracted/modrinth.index.json"));
    QFile indexFile(m_indexPath);
    if (!indexFile.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("整合包中缺少 modrinth.index.json。"));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(indexFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("整合包索引解析失败：%1").arg(parseError.errorString()));
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("formatVersion")).toInt() != 1
        || root.value(QStringLiteral("game")).toString() != QStringLiteral("minecraft")) {
        fail(QStringLiteral("不支持此整合包格式。"));
        return;
    }
    const QJsonObject dependencies = root.value(QStringLiteral("dependencies")).toObject();
    m_minecraftVersion =
        dependencies.value(QStringLiteral("minecraft")).toString().trimmed();
    if (!validVersionPart(m_minecraftVersion)) {
        fail(QStringLiteral("整合包未指定有效的 Minecraft 版本。"));
        return;
    }
    m_loaderName.clear();
    m_loaderVersion.clear();
    const QStringList loaderKeys = {
        QStringLiteral("fabric-loader"), QStringLiteral("forge"),
        QStringLiteral("neoforge")
    };
    for (const QString &key : loaderKeys) {
        const QString version = dependencies.value(key).toString().trimmed();
        if (version.isEmpty()) {
            continue;
        }
        if (!m_loaderName.isEmpty()) {
            fail(QStringLiteral("整合包同时声明了多个加载器，无法确定安装配置。"));
            return;
        }
        m_loaderName = key == QStringLiteral("fabric-loader")
            ? QStringLiteral("Fabric")
            : key == QStringLiteral("neoforge")
                ? QStringLiteral("NeoForge")
                : QStringLiteral("Forge");
        m_loaderVersion = version;
        if (!validVersionPart(m_loaderVersion)) {
            fail(QStringLiteral("整合包指定的加载器版本无效。"));
            return;
        }
    }
    for (const QString &key : dependencies.keys()) {
        if (key == QStringLiteral("minecraft") || loaderKeys.contains(key)) {
            continue;
        }
        if (!dependencies.value(key).toString().isEmpty()) {
            fail(QStringLiteral("暂不支持整合包加载器：%1").arg(key));
            return;
        }
    }

    const QString overrides = root.value(QStringLiteral("overrides"))
                                  .toString(QStringLiteral("overrides"));
    const QString safeOverrides = safeRelativePath(overrides);
    if (safeOverrides.isEmpty()) {
        fail(QStringLiteral("整合包覆盖目录路径无效。"));
        return;
    }
    m_overrideDirectory = QDir(m_temporaryDirectory->path())
                             .filePath(QStringLiteral("extracted/%1").arg(safeOverrides));
    m_downloadPaths.clear();
    m_expectedSizes.clear();
    m_expectedSha512.clear();
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    m_stagingModsDirectory =
        QDir(m_temporaryDirectory->path()).filePath(QStringLiteral("downloaded-files"));
    for (const QJsonValue &value : files) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject file = value.toObject();
        const QJsonObject environment = file.value(QStringLiteral("env")).toObject();
        if (environment.value(QStringLiteral("client")).toString()
            == QStringLiteral("unsupported")) {
            continue;
        }
        const QString relativePath =
            safeRelativePath(file.value(QStringLiteral("path")).toString());
        if (relativePath.isEmpty()) {
            fail(QStringLiteral("整合包包含无效文件路径。"));
            return;
        }
        const QJsonArray downloads = file.value(QStringLiteral("downloads")).toArray();
        QUrl downloadUrl;
        for (const QJsonValue &candidate : downloads) {
            const QUrl url(candidate.toString());
            if (url.isValid() && !url.host().isEmpty()
                && (url.scheme() == QStringLiteral("https")
                    || url.scheme() == QStringLiteral("http"))) {
                downloadUrl = url;
                break;
            }
        }
        if (!downloadUrl.isValid()) {
            fail(QStringLiteral("整合包文件缺少有效的下载链接：%1").arg(relativePath));
            return;
        }
        const QJsonObject hashes = file.value(QStringLiteral("hashes")).toObject();
        const QString sha1 = hashes.value(QStringLiteral("sha1")).toString().trimmed();
        const QString sha512 = hashes.value(QStringLiteral("sha512")).toString().trimmed();
        if ((!sha1.isEmpty() && !validSha(sha1, 40))
            || (sha1.isEmpty() && !validSha(sha512, 128))) {
            fail(QStringLiteral("整合包文件缺少有效的 SHA-1/SHA-512：%1")
                     .arg(relativePath));
            return;
        }
        const qint64 size = static_cast<qint64>(
            file.value(QStringLiteral("fileSize")).toDouble(-1));
        if (size < 0) {
            fail(QStringLiteral("整合包文件缺少有效大小：%1").arg(relativePath));
            return;
        }
        const QString target = QDir(m_stagingModsDirectory).filePath(relativePath);
        m_downloadPaths.append(target);
        m_expectedSizes.append(size);
        m_expectedSha512.append(sha1.isEmpty() ? sha512.toLatin1() : QByteArray());
        m_pendingDownloads.append(
            new DownloadTask(downloadUrl, target, sha1.toLatin1(), this));
    }
    installBaseGame();
}

void ModpackInstaller::installBaseGame()
{
    if (m_cancelled) {
        finishCancelled();
        return;
    }
    const QString clientJar = QDir(m_gameDirectory).filePath(
        QStringLiteral("versions/%1/%1.jar").arg(m_minecraftVersion));
    if (QFileInfo::exists(jsonVersionPath(m_gameDirectory, m_minecraftVersion))
        && QFileInfo::exists(clientJar)) {
        installLoader();
        return;
    }

    m_phase = Phase::InstallingGame;
    emit phaseChanged(QStringLiteral("正在下载 Minecraft %1").arg(m_minecraftVersion));
    emit percentChanged(10);
    m_gameInstaller = new GameInstaller(m_gameDirectory, this);
    connect(m_gameInstaller, &GameInstaller::phaseChanged, this,
            [this](const QString &phase) { emit phaseChanged(phase); });
    connect(m_gameInstaller, &GameInstaller::overallPercentChanged, this,
            [this](int value) { emit percentChanged(10 + qBound(0, value, 100) / 5); });
    connect(m_gameInstaller, &GameInstaller::succeeded, this,
            [this](const QString &) { installLoader(); });
    connect(m_gameInstaller, &GameInstaller::failed, this,
            [this](const QString &reason) {
        if (m_cancelled) {
            finishCancelled();
        } else {
            fail(QStringLiteral("Minecraft 下载失败：%1").arg(reason));
        }
    });
    connect(m_gameInstaller, &GameInstaller::cancelled,
            this, &ModpackInstaller::finishCancelled);
    m_gameInstaller->install(m_minecraftVersion);
}

void ModpackInstaller::installLoader()
{
    if (m_cancelled) {
        finishCancelled();
        return;
    }
    if (m_loaderName.isEmpty()) {
        m_installedLoaderId = m_minecraftVersion;
        downloadModFiles();
        return;
    }

    LoaderSelection selection;
    selection.loader = m_loaderName;
    selection.loaderVersion = m_loaderVersion;
    if (m_loaderName == QStringLiteral("Forge")) {
        const QString coordinate =
            QStringLiteral("%1-%2").arg(m_minecraftVersion, m_loaderVersion);
        selection.downloadUrl =
            QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/"
                           "%1/forge-%1-installer.jar").arg(coordinate);
        selection.downloadFilename =
            QStringLiteral("forge-%1-installer.jar").arg(coordinate);
    } else if (m_loaderName == QStringLiteral("NeoForge")) {
        selection.downloadUrl =
            QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/"
                           "%1/neoforge-%1-installer.jar").arg(m_loaderVersion);
        selection.downloadFilename =
            QStringLiteral("neoforge-%1-installer.jar").arg(m_loaderVersion);
    }

    m_phase = Phase::InstallingLoader;
    emit phaseChanged(QStringLiteral("正在下载加载器"));
    emit percentChanged(30);
    m_loaderDownloader = new LoaderDownloader(m_gameDirectory, this);
    connect(m_loaderDownloader, &LoaderDownloader::phaseChanged, this,
            [this](const QString &phase) {
        emit phaseChanged(phase);
    });
    connect(m_loaderDownloader, &LoaderDownloader::percentChanged, this,
            [this](int value) { emit percentChanged(30 + qBound(0, value, 100) / 5); });
    connect(m_loaderDownloader, &LoaderDownloader::finished, this,
            [this](const QString &) {
        m_installedLoaderId = m_loaderDownloader->installedVersionId();
        if (m_installedLoaderId.isEmpty()) {
            fail(QStringLiteral("加载器安装完成，但没有找到对应版本。"));
            return;
        }
        downloadModFiles();
    });
    connect(m_loaderDownloader, &LoaderDownloader::failed, this,
            [this](const QString &reason) {
        if (m_cancelled) {
            finishCancelled();
        } else {
            fail(QStringLiteral("加载器安装失败：%1").arg(reason));
        }
    });
    connect(m_loaderDownloader, &LoaderDownloader::cancelled,
            this, &ModpackInstaller::finishCancelled);
    m_loaderDownloader->start(m_minecraftVersion, selection);
}

void ModpackInstaller::downloadModFiles()
{
    if (m_cancelled) {
        finishCancelled();
        return;
    }
    if (m_phase == Phase::DownloadingFiles) {
        if (m_downloadFailures > 0) {
            fail(QStringLiteral("整合包文件下载失败：%1").arg(m_lastFailure));
            return;
        }
        for (int i = 0; i < m_downloadPaths.size(); ++i) {
            const QFileInfo file(m_downloadPaths.at(i));
            if (!file.exists() || file.size() != m_expectedSizes.at(i)) {
                fail(QStringLiteral("整合包文件大小校验失败：%1").arg(file.fileName()));
                return;
            }
            if (!m_expectedSha512.at(i).isEmpty()) {
                QFile input(file.absoluteFilePath());
                if (!input.open(QIODevice::ReadOnly)) {
                    fail(QStringLiteral("无法校验整合包文件：%1").arg(file.fileName()));
                    return;
                }
                QCryptographicHash hash(QCryptographicHash::Sha512);
                while (!input.atEnd()) {
                    hash.addData(input.read(1024 * 1024));
                }
                if (hash.result().toHex().compare(m_expectedSha512.at(i),
                                                  Qt::CaseInsensitive) != 0) {
                    fail(QStringLiteral("整合包文件 SHA-512 校验失败：%1")
                             .arg(file.fileName()));
                    return;
                }
            }
        }
        createInstance();
        return;
    }
    if (m_pendingDownloads.isEmpty()) {
        createInstance();
        return;
    }
    m_phase = Phase::DownloadingFiles;
    m_downloadFailures = 0;
    m_lastFailure.clear();
    emit phaseChanged(QStringLiteral("正在下载Mod (0/%1)").arg(m_pendingDownloads.size()));
    emit modProgressChanged(0, m_pendingDownloads.size());
    emit percentChanged(45);
    if (!m_manager->start(m_pendingDownloads)) {
        for (DownloadTask *task : std::as_const(m_pendingDownloads)) {
            delete task;
        }
        m_pendingDownloads.clear();
        fail(QStringLiteral("无法启动整合包文件下载。"));
        return;
    }
    m_pendingDownloads.clear();
}

void ModpackInstaller::createInstance()
{
    m_phase = Phase::CreatingInstance;
    emit phaseChanged(QStringLiteral("正在创建整合包版本"));
    emit percentChanged(90);

    const QString candidateVersionId = instanceVersionId();
    if (candidateVersionId.isEmpty()) {
        fail(QStringLiteral("无法生成安全的整合包版本名称。"));
        return;
    }
    if (QFileInfo::exists(jsonVersionPath(m_gameDirectory, candidateVersionId))
        || QDir(m_gameDirectory).exists(
            QStringLiteral("versions/%1").arg(candidateVersionId))) {
        fail(QStringLiteral("整合包版本名称冲突，请重试安装。"));
        return;
    }
    m_versionId = candidateVersionId;
    m_versionDirectory = QDir(m_gameDirectory).filePath(
        QStringLiteral("versions/%1").arg(m_versionId));
    if (!QDir().mkpath(m_versionDirectory)
        || !QDir().mkpath(QDir(m_versionDirectory).filePath(QStringLiteral("mods")))) {
        fail(QStringLiteral("无法创建整合包版本目录。"));
        return;
    }

    QJsonObject profile;
    if (m_installedLoaderId != m_minecraftVersion) {
        QFile source(jsonVersionPath(m_gameDirectory, m_installedLoaderId));
        if (!source.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("无法读取加载器版本配置。"));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(source.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fail(QStringLiteral("加载器版本配置无效：%1").arg(parseError.errorString()));
            return;
        }
        profile = document.object();
        profile.insert(QStringLiteral("inheritsFrom"), m_minecraftVersion);
    } else {
        profile.insert(QStringLiteral("inheritsFrom"), m_minecraftVersion);
        profile.insert(QStringLiteral("type"), QStringLiteral("release"));
    }
    profile.insert(QStringLiteral("id"), m_versionId);

    QSaveFile profileFile(jsonVersionPath(m_gameDirectory, m_versionId));
    const QByteArray profileData =
        QJsonDocument(profile).toJson(QJsonDocument::Indented);
    if (!profileFile.open(QIODevice::WriteOnly)
        || profileFile.write(profileData) != profileData.size()
        || !profileFile.commit()) {
        fail(QStringLiteral("无法写入整合包版本配置。"));
        return;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("displayName"), m_displayName);
    metadata.insert(QStringLiteral("modpack"), true);
    QSaveFile metadataFile(QDir(m_versionDirectory).filePath(
        QStringLiteral("launcher.json")));
    const QByteArray metadataData =
        QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if (!metadataFile.open(QIODevice::WriteOnly)
        || metadataFile.write(metadataData) != metadataData.size()
        || !metadataFile.commit()) {
        fail(QStringLiteral("无法写入整合包版本元数据。"));
        return;
    }

    for (const QString &sourcePath : std::as_const(m_downloadPaths)) {
        const QString relativePath =
            safeRelativePath(QDir(m_stagingModsDirectory).relativeFilePath(sourcePath));
        if (relativePath.isEmpty()) {
            fail(QStringLiteral("整合包文件路径校验失败。"));
            return;
        }
        const QString destination = QDir(m_versionDirectory).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
            fail(QStringLiteral("无法创建整合包文件目录。"));
            return;
        }
        if (QFileInfo::exists(destination)) {
            QFile::remove(destination);
        }
        if (!QFile::copy(sourcePath, destination)) {
            fail(QStringLiteral("无法安装整合包文件：%1")
                     .arg(QFileInfo(sourcePath).fileName()));
            return;
        }
    }

    QString overrideError;
    if (QDir(m_overrideDirectory).exists()
        && !copyOverrides(m_overrideDirectory, m_versionDirectory, &overrideError)) {
        fail(QStringLiteral("复制整合包覆盖文件失败：%1").arg(overrideError));
        return;
    }

    m_busy = false;
    emit percentChanged(100);
    emit phaseChanged(QStringLiteral("整合包安装完成"));
    emit finished(m_versionId);
    delete m_temporaryDirectory;
    m_temporaryDirectory = nullptr;
}

bool ModpackInstaller::copyOverrides(const QString &sourceDirectory,
                                    const QString &targetDirectory,
                                    QString *errorMessage)
{
    const QDir source(sourceDirectory);
    QDirIterator iterator(sourceDirectory, QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QString relativePath = safeRelativePath(source.relativeFilePath(sourcePath));
        if (relativePath.isEmpty()
            || relativePath.compare(QStringLiteral("launcher.json"),
                                    Qt::CaseInsensitive) == 0
            || relativePath.compare(QStringLiteral("%1.json").arg(m_versionId),
                                    Qt::CaseInsensitive) == 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("覆盖文件路径不允许：%1").arg(sourcePath);
            }
            return false;
        }
        const QString destination = QDir(targetDirectory).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建目录：%1")
                                    .arg(QFileInfo(destination).absolutePath());
            }
            return false;
        }
        if (QFileInfo::exists(destination)) {
            QFile::remove(destination);
        }
        if (!QFile::copy(sourcePath, destination)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法复制文件：%1").arg(relativePath);
            }
            return false;
        }
    }
    return true;
}

QString ModpackInstaller::instanceVersionId() const
{
    const QString base = QStringLiteral("%1-%2")
                             .arg(safeInstanceName(m_displayName).left(36),
                                  m_minecraftVersion.left(24));
    return QStringLiteral("%1-%2")
        .arg(base, QUuid::createUuid().toString(QUuid::Id128).left(8));
}

void ModpackInstaller::fail(const QString &reason)
{
    if (!m_busy) {
        return;
    }
    if (m_packTask && !m_packTask->isFinished()) {
        m_packTask->cancel();
    }
    if (m_manager->isRunning()) {
        m_manager->cancel();
    }
    if (m_gameInstaller && m_gameInstaller->isBusy()) {
        m_gameInstaller->cancel();
    }
    if (m_loaderDownloader && m_loaderDownloader->isBusy()) {
        m_loaderDownloader->cancel();
    }
    if (m_extractProcess && m_extractProcess->state() != QProcess::NotRunning) {
        m_extractProcess->kill();
    }
    m_busy = false;
    qDeleteAll(m_pendingDownloads);
    m_pendingDownloads.clear();
    if (!m_versionId.isEmpty()) {
        QDir(m_versionDirectory).removeRecursively();
        QFile::remove(jsonVersionPath(m_gameDirectory, m_versionId));
    }
    emit failed(reason);
    delete m_temporaryDirectory;
    m_temporaryDirectory = nullptr;
}

void ModpackInstaller::reset()
{
    delete m_temporaryDirectory;
    m_temporaryDirectory = nullptr;
    m_displayName.clear();
    m_minecraftVersion.clear();
    m_loaderName.clear();
    m_loaderVersion.clear();
    m_installedLoaderId.clear();
    m_overrideDirectory.clear();
    m_indexPath.clear();
    m_stagingModsDirectory.clear();
    m_versionId.clear();
    m_versionDirectory.clear();
    m_downloadPaths.clear();
    qDeleteAll(m_pendingDownloads);
    m_pendingDownloads.clear();
    m_expectedSizes.clear();
    m_expectedSha512.clear();
    m_downloadFailures = 0;
    m_lastFailure.clear();
    m_cancelled = false;
    m_phase = Phase::Idle;
}

void ModpackInstaller::finishCancelled()
{
    if (!m_busy) {
        return;
    }
    m_busy = false;
    m_phase = Phase::Idle;
    qDeleteAll(m_pendingDownloads);
    m_pendingDownloads.clear();
    if (!m_versionId.isEmpty()) {
        QDir(m_versionDirectory).removeRecursively();
        QFile::remove(jsonVersionPath(m_gameDirectory, m_versionId));
    }
    emit phaseChanged(QStringLiteral("整合包安装已取消"));
    emit cancelled();
    delete m_temporaryDirectory;
    m_temporaryDirectory = nullptr;
}
