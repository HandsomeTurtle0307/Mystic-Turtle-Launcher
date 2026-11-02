/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "GameLauncher.h"

#include "DownloadManager.h"
#include "DownloadTask.h"
#include "MinecraftPaths.h"
#include "account/AccountManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>
#include <QVector>
#include <utility>

namespace {

QString currentOsName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("osx");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}

QString currentArchName()
{
    QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch == QStringLiteral("i386") || arch == QStringLiteral("i686")) {
        return QStringLiteral("x86");
    }
    if (arch == QStringLiteral("amd64")) {
        return QStringLiteral("x86_64");
    }
    return arch;
}

bool ruleMatches(const QJsonObject &rule)
{
    const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
    if (!os.isEmpty()) {
        const QString name = os.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()
            && name.compare(currentOsName(), Qt::CaseInsensitive) != 0) {
            return false;
        }
        const QString arch = os.value(QStringLiteral("arch")).toString();
        if (!arch.isEmpty()
            && arch.compare(currentArchName(), Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    const QJsonObject features = rule.value(QStringLiteral("features")).toObject();
    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        if (it.value().toBool()) {
            return false;
        }
    }
    return true;
}

bool rulesAllow(const QJsonArray &rules)
{
    if (rules.isEmpty()) {
        return true;
    }

    bool allowed = false;
    for (const QJsonValue &value : rules) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject rule = value.toObject();
        if (!ruleMatches(rule)) {
            continue;
        }
        const QString action = rule.value(QStringLiteral("action")).toString();
        if (action == QStringLiteral("allow")) {
            allowed = true;
        } else if (action == QStringLiteral("disallow")) {
            allowed = false;
        }
    }
    return allowed;
}

QString mavenPath(const QString &name)
{
    QString coordinate = name.trimmed();
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

QString expandedNativeClassifier(QString classifier)
{
    if (classifier.contains(QStringLiteral("${arch}"))) {
        const bool is64Bit = QSysInfo::currentCpuArchitecture().contains(
            QStringLiteral("64"), Qt::CaseInsensitive);
        classifier.replace(QStringLiteral("${arch}"),
                           is64Bit ? QStringLiteral("64") : QStringLiteral("32"));
    }
    return classifier;
}

QString mavenModuleKey(const QString &name)
{
    QString coordinate = name.trimmed();
    const qsizetype extensionAt = coordinate.lastIndexOf(QLatin1Char('@'));
    if (extensionAt >= 0) {
        coordinate.truncate(extensionAt);
    }

    const QStringList parts = coordinate.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }
    return parts.at(0) + QLatin1Char(':') + parts.at(1);
}

struct LibraryDownload {
    QString path;
    QString url;
    QString sha1;
    qint64 size = -1;
    bool verifyExistingSha1 = true;
};

bool fileMatchesLibrary(const QString &path, const LibraryDownload &library)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()
        || (library.size >= 0 && info.size() != library.size)) {
        return false;
    }
    if (library.sha1.isEmpty() || !library.verifyExistingSha1) {
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        return false;
    }
    return QString::fromLatin1(hash.result().toHex())
               .compare(library.sha1, Qt::CaseInsensitive) == 0;
}

bool prepareFirstLaunchOptions(const QString &gameDataDirectory,
                               QString *errorMessage)
{
    const QString optionsPath =
        QDir(gameDataDirectory).filePath(QStringLiteral("options.txt"));
    QStringList lines;
    QFile optionsFile(optionsPath);
    if (optionsFile.exists()) {
        if (!optionsFile.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取游戏选项：%1")
                                    .arg(optionsFile.errorString());
            }
            return false;
        }
        const QString content = QString::fromUtf8(optionsFile.readAll());
        lines = content.split(QLatin1Char('\n'));
        if (!lines.isEmpty() && lines.last().isEmpty()) {
            lines.removeLast();
        }
    }

    bool hasLanguage = false;
    bool hasNarrator = false;
    bool hasAccessibilityOnboarding = false;
    bool changed = false;
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        const qsizetype separator = line.indexOf(QLatin1Char(':'));
        if (separator <= 0) {
            continue;
        }
        const QString key = line.left(separator);
        if (key == QStringLiteral("lang")) {
            hasLanguage = true;
        } else if (key == QStringLiteral("narrator")) {
            hasNarrator = true;
        } else if (key == QStringLiteral("onboardAccessibility")) {
            hasAccessibilityOnboarding = true;
            if (line.mid(separator + 1).trimmed()
                    .compare(QStringLiteral("true"), Qt::CaseInsensitive) != 0) {
                line = QStringLiteral("onboardAccessibility:true");
                changed = true;
            }
        }
    }

    if (!hasLanguage) {
        lines.append(QStringLiteral("lang:zh_cn"));
        changed = true;
    }
    if (!hasNarrator) {
        lines.append(QStringLiteral("narrator:0"));
        changed = true;
    }
    if (!hasAccessibilityOnboarding) {
        lines.append(QStringLiteral("onboardAccessibility:true"));
        changed = true;
    }
    if (!changed) {
        return true;
    }

    QSaveFile output(optionsPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法保存游戏选项：%1")
                                .arg(output.errorString());
        }
        return false;
    }
    const QByteArray contents = (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    if (output.write(contents) != contents.size() || !output.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入游戏选项：%1")
                                .arg(output.errorString());
        }
        return false;
    }
    return true;
}

bool ensureGameFiles(const QString &gameDirectory,
                     const QList<LibraryDownload> &files,
                     const GameLauncher::ProgressCallback &progress,
                     const GameLauncher::CancelCallback &cancel,
                     QString *errorMessage,
                     int progressBase = 45,
                     int progressSpan = 8,
                     const QString &progressLabel = QStringLiteral("正在补全游戏文件…"))
{
    QList<DownloadTask *> tasks;
    QSet<QString> queuedPaths;

    for (const LibraryDownload &library : files) {
        QString relativePath = library.path;
        relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString cleanPath = QDir::cleanPath(relativePath);
        const bool allowedGamePath =
            cleanPath.startsWith(QStringLiteral("libraries/"))
            || cleanPath.startsWith(QStringLiteral("assets/objects/"))
            || cleanPath.startsWith(QStringLiteral("assets/indexes/"))
            || cleanPath.startsWith(QStringLiteral("assets/log_configs/"))
            || cleanPath.startsWith(QStringLiteral("versions/"));
        if (cleanPath.isEmpty() || cleanPath == QStringLiteral(".")
            || cleanPath == QStringLiteral("..")
            || cleanPath.startsWith(QStringLiteral("../"))
            || QDir::isAbsolutePath(cleanPath)
            || cleanPath.contains(QLatin1Char(':'))
            || !allowedGamePath) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("版本 JSON 中的游戏文件路径无效：%1")
                                    .arg(library.path);
            }
            qDeleteAll(tasks);
            return false;
        }

        const QString absolutePath = QDir(gameDirectory).filePath(cleanPath);
        if (fileMatchesLibrary(absolutePath, library)) {
            continue;
        }
        if (queuedPaths.contains(cleanPath)) {
            continue;
        }
        queuedPaths.insert(cleanPath);

        const QUrl url(library.url);
        if (!url.isValid()
            || (url.scheme() != QStringLiteral("http")
                && url.scheme() != QStringLiteral("https"))
            || url.host().isEmpty()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("游戏文件缺失或损坏，且版本 JSON 没有提供有效下载地址：%1")
                        .arg(cleanPath);
            }
            qDeleteAll(tasks);
            return false;
        }
        if (!library.sha1.isEmpty() && library.sha1.size() != 40) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("游戏文件 SHA-1 格式无效：%1").arg(cleanPath);
            }
            qDeleteAll(tasks);
            return false;
        }
        if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath())) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建游戏文件目录：%1")
                                    .arg(QFileInfo(absolutePath).absolutePath());
            }
            qDeleteAll(tasks);
            return false;
        }

        tasks.append(new DownloadTask(url, absolutePath, library.sha1.toLatin1()));
    }

    if (tasks.isEmpty()) {
        return true;
    }
    if (cancel && cancel()) {
        qDeleteAll(tasks);
        if (errorMessage) {
            *errorMessage = QStringLiteral("已取消启动。");
        }
        return false;
    }

    DownloadManager manager;
    QEventLoop loop;
    QTimer cancelTimer;
    cancelTimer.setInterval(100);
    int failures = 0;
    QString lastFailure;
    QObject::connect(&manager, &DownloadManager::taskFailed, &loop,
                     [&failures, &lastFailure](int, const QString &reason) {
        ++failures;
        lastFailure = reason;
    });
    QObject::connect(&manager, &DownloadManager::overallPercentChanged, &loop,
                     [&progress, progressBase, progressSpan, progressLabel](int percent) {
        if (progress) {
            progress(progressBase + qRound(percent * progressSpan / 100.0),
                     QStringLiteral("%1 %2%").arg(progressLabel).arg(percent));
        }
    });
    QObject::connect(&manager, &DownloadManager::finished, &loop, &QEventLoop::quit);
    QObject::connect(&cancelTimer, &QTimer::timeout, &manager,
                     [&manager, &cancelTimer, &cancel]() {
        if (cancel && cancel()) {
            cancelTimer.stop();
            manager.cancel();
        }
    });

    if (!manager.start(tasks)) {
        qDeleteAll(tasks);
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法启动游戏文件下载任务。");
        }
        return false;
    }

    cancelTimer.start();
    loop.exec();
    cancelTimer.stop();

    if (cancel && cancel()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("已取消启动。");
        }
        return false;
    }
    if (failures > 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("游戏文件下载失败（%1 个）：%2")
                                .arg(failures)
                                .arg(lastFailure);
        }
        return false;
    }
    for (const LibraryDownload &library : files) {
        QString relativePath = library.path;
        relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString path = QDir(gameDirectory).filePath(QDir::cleanPath(relativePath));
        if (!fileMatchesLibrary(path, library)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("游戏文件下载后校验失败：%1")
                                    .arg(relativePath);
            }
            return false;
        }
    }
    return true;
}

QString offlineUuid(const QString &username)
{
    QByteArray digest = QCryptographicHash::hash(
        QStringLiteral("OfflinePlayer:%1").arg(username).toUtf8(),
        QCryptographicHash::Md5);
    if (digest.size() != 16) {
        return QStringLiteral("00000000-0000-3000-8000-000000000000");
    }
    digest[6] = static_cast<char>((digest.at(6) & 0x0f) | 0x30);
    digest[8] = static_cast<char>((digest.at(8) & 0x3f) | 0x80);
    const QString hex = QString::fromLatin1(digest.toHex());
    return hex.mid(0, 8) + QLatin1Char('-') + hex.mid(8, 4) + QLatin1Char('-')
        + hex.mid(12, 4) + QLatin1Char('-') + hex.mid(16, 4) + QLatin1Char('-')
        + hex.mid(20, 12);
}

QString substitute(QString text, const QHash<QString, QString> &values)
{
    static const QRegularExpression placeholder(
        QStringLiteral("\\$\\{([^}]+)\\}"));
    int offset = 0;
    while (true) {
        const QRegularExpressionMatch match = placeholder.match(text, offset);
        if (!match.hasMatch()) {
            break;
        }
        const QString key = match.captured(1);
        const QString replacement = values.value(key);
        text.replace(match.capturedStart(), match.capturedLength(), replacement);
        offset = match.capturedStart() + replacement.size();
    }
    return text;
}

void appendArgumentValue(const QJsonValue &value, QStringList *output,
                         const QHash<QString, QString> &values)
{
    if (value.isString()) {
        output->append(substitute(value.toString(), values));
    } else if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            if (item.isString()) {
                output->append(substitute(item.toString(), values));
            }
        }
    }
}

void appendArgumentArray(const QJsonArray &array, QStringList *output,
                         const QHash<QString, QString> &values)
{
    for (const QJsonValue &value : array) {
        if (value.isString()) {
            output->append(substitute(value.toString(), values));
            continue;
        }
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        if (!rulesAllow(object.value(QStringLiteral("rules")).toArray())) {
            continue;
        }
        appendArgumentValue(object.value(QStringLiteral("value")), output, values);
    }
}

bool readVersionChain(const QString &gameDirectory, const QString &versionId,
                      QVector<QJsonObject> *chain, QString *errorMessage)
{
    QString current = versionId;
    QSet<QString> visited;
    const QDir versionsDirectory(QDir(gameDirectory).filePath(QStringLiteral("versions")));

    while (!current.isEmpty()) {
        if (visited.contains(current)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("版本继承关系存在循环：%1").arg(current);
            }
            return false;
        }
        visited.insert(current);

        const QString jsonPath = versionsDirectory.filePath(
            QStringLiteral("%1/%1.json").arg(current));
        QFile file(jsonPath);
        if (!file.exists()) {
            const QFileInfoList jsonFiles = QDir(
                versionsDirectory.filePath(current))
                .entryInfoList(QStringList{QStringLiteral("*.json")},
                               QDir::Files, QDir::Name | QDir::IgnoreCase);
            if (!jsonFiles.isEmpty()) {
                file.setFileName(jsonFiles.first().absoluteFilePath());
            }
        }
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取版本 JSON：%1").arg(file.fileName());
            }
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("版本 JSON 解析失败：%1").arg(jsonPath);
            }
            return false;
        }

        const QJsonObject object = document.object();
        chain->append(object);
        current = object.value(QStringLiteral("inheritsFrom")).toString().trimmed();
    }
    return !chain->isEmpty();
}

QString nativeJarRelativePath(const QJsonObject &library)
{
    const QString name = library.value(QStringLiteral("name")).toString();
    const QJsonObject downloads = library.value(QStringLiteral("downloads")).toObject();

    const QJsonObject natives = library.value(QStringLiteral("natives")).toObject();
    const QString classifier =
        expandedNativeClassifier(natives.value(currentOsName()).toString().trimmed());
    if (!classifier.isEmpty()) {
        const QJsonObject classifiers =
            downloads.value(QStringLiteral("classifiers")).toObject();
        const QString path = classifiers.value(classifier).toObject()
                                 .value(QStringLiteral("path")).toString();
        if (!path.isEmpty()) {
            return path;
        }
        return mavenPath(name + QLatin1Char(':') + classifier);
    }

    const QString artifactPath = downloads.value(QStringLiteral("artifact")).toObject()
                                     .value(QStringLiteral("path")).toString();
    if (artifactPath.contains(QStringLiteral("natives-windows"),
                              Qt::CaseInsensitive)) {
        return artifactPath;
    }
    const QString lowerName = name.toLower();
    if (lowerName.contains(QStringLiteral(":natives-windows"))) {
        const QString maven = mavenPath(name);
        if (!maven.isEmpty()) {
            return maven;
        }
    }
    return QString();
}

QString findInRuntime(const QString &root)
{
    QDir directory(root);
    if (!directory.exists()) {
        return QString();
    }

    QString bestJavaw;
    QDateTime bestJavawTime;
    QString bestJava;
    QDateTime bestJavaTime;
    QDirIterator iterator(root,
                          QStringList{QStringLiteral("javaw.exe"),
                                      QStringLiteral("java.exe")},
                          QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        if (info.fileName().compare(QStringLiteral("javaw.exe"),
                                    Qt::CaseInsensitive) == 0) {
            if (bestJavaw.isEmpty() || info.lastModified() > bestJavawTime) {
                bestJavaw = path;
                bestJavawTime = info.lastModified();
            }
        } else if (bestJava.isEmpty() || info.lastModified() > bestJavaTime) {
            bestJava = path;
            bestJavaTime = info.lastModified();
        }
    }

    return bestJavaw.isEmpty() ? bestJava : bestJavaw;
}

bool extractZipArchive(const QString &archivePath, const QString &destination,
                       QString *errorMessage)
{
    const QString tar = !QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty()
        ? QStandardPaths::findExecutable(QStringLiteral("tar"))
        : QStringLiteral("C:/Windows/System32/tar.exe");
    if (!QFileInfo::exists(tar)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到 Windows tar，无法解压本地依赖库。");
        }
        return false;
    }

    if (!QDir().mkpath(destination)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建 natives 目录：%1").arg(destination);
        }
        return false;
    }

    QProcess process;
    process.start(tar, QStringList{
                             QStringLiteral("-xf"), archivePath,
                             QStringLiteral("-C"), destination});
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = QStringLiteral("解压本地依赖库超时：%1")
                                .arg(QFileInfo(archivePath).fileName());
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            const QString detail =
                QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorMessage = QStringLiteral("无法解压本地依赖库：%1%2")
                                .arg(QFileInfo(archivePath).fileName(),
                                     detail.isEmpty() ? QString()
                                                      : QStringLiteral("（%1）").arg(detail));
        }
        return false;
    }
    return true;
}

bool flattenWindowsNativeLibraries(const QString &nativesDirectory,
                                   QString *errorMessage)
{
    const QDir root(nativesDirectory);
    QDirIterator iterator(nativesDirectory, QStringList{QStringLiteral("*.dll")},
                          QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        if (QDir::cleanPath(QFileInfo(sourcePath).absolutePath())
            == QDir::cleanPath(root.absolutePath())) {
            continue;
        }

        const QString destinationPath =
            root.filePath(QFileInfo(sourcePath).fileName());
        if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法准备本地依赖库：%1")
                                    .arg(QFileInfo(sourcePath).fileName());
            }
            return false;
        }
        if (!QFile::copy(sourcePath, destinationPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法展平本地依赖库：%1")
                                    .arg(QFileInfo(sourcePath).fileName());
            }
            return false;
        }
    }
    return true;
}

QString javaVersionProbePath(const QString &javaExecutable)
{
    const QFileInfo info(javaExecutable);
    if (info.fileName().compare(QStringLiteral("javaw.exe"), Qt::CaseInsensitive) == 0) {
        const QString sibling = info.absoluteDir().filePath(QStringLiteral("java.exe"));
        if (QFileInfo::exists(sibling)) {
            return sibling;
        }
    }
    return javaExecutable;
}

QString javawIfAvailable(const QString &javaExecutable)
{
    const QFileInfo info(javaExecutable);
    if (info.fileName().compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0) {
        const QString javaw = info.absoluteDir().filePath(QStringLiteral("javaw.exe"));
        if (QFileInfo::exists(javaw)) {
            return javaw;
        }
    }
    return javaExecutable;
}

int inferJavaMajorFromVersionId(const QString &versionId)
{
    static const QRegularExpression versionPattern(
        QStringLiteral("^1\\.(\\d+)(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = versionPattern.match(versionId.trimmed());
    if (!match.hasMatch()) {
        return 0;
    }

    const int minor = match.captured(1).toInt();
    const int patch = match.captured(2).toInt();
    if (minor >= 21 || (minor == 20 && patch >= 5)) {
        return 21;
    }
    if (minor >= 18) {
        return 17;
    }
    if (minor == 17) {
        return 16;
    }
    return 8;
}

int detectJavaMajorVersion(const QString &javaExecutable, QString *diagnostic = nullptr)
{
    if (diagnostic) {
        diagnostic->clear();
    }
    const QString consoleJava = javaVersionProbePath(javaExecutable);
    const QFileInfo info(consoleJava);
    if (consoleJava.trimmed().isEmpty() || !info.exists() || !info.isFile()) {
        const QString message = consoleJava.trimmed().isEmpty()
            ? QStringLiteral("Java 路径为空。")
            : QStringLiteral("Java 可执行文件不存在：%1").arg(consoleJava);
        if (diagnostic) {
            *diagnostic = message;
        }
        qWarning().noquote() << "[Java probe]" << message;
        return 0;
    }
    const QString cacheKey = info.canonicalFilePath().isEmpty()
        ? info.absoluteFilePath()
        : info.canonicalFilePath();
    const qint64 fileSize = info.size();
    const qint64 modified = info.lastModified().toMSecsSinceEpoch();

    struct CachedVersion {
        qint64 fileSize = -1;
        qint64 modified = -1;
        int major = 0;
    };
    static QMutex cacheMutex;
    static QHash<QString, CachedVersion> versionCache;
    {
        const QMutexLocker locker(&cacheMutex);
        const auto cached = versionCache.constFind(cacheKey);
        if (cached != versionCache.constEnd()
            && cached->fileSize == fileSize
            && cached->modified == modified) {
            return cached->major;
        }
    }

    QProcess process;
    process.start(consoleJava, QStringList{QStringLiteral("-version")});
    if (!process.waitForStarted(2000)) {
        const QString message =
            QStringLiteral("无法启动 Java 版本检测进程：%1\n标准错误：%2")
                .arg(process.errorString(),
                     QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        if (diagnostic) {
            *diagnostic = message;
        }
        qWarning().noquote() << "[Java probe]" << consoleJava << message;
        return 0;
    }
    if (!process.waitForFinished(3500)) {
        process.kill();
        process.waitForFinished(1000);
        const QString stderrOutput =
            QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        const QString message =
            QStringLiteral("Java 版本检测超时：%1\n标准错误：%2")
                .arg(process.errorString(),
                     stderrOutput.isEmpty() ? QStringLiteral("（空）") : stderrOutput);
        if (diagnostic) {
            *diagnostic = message;
        }
        qWarning().noquote() << "[Java probe]" << consoleJava << message;
        return 0;
    }

    const QByteArray standardError = process.readAllStandardError();
    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray output = standardError + standardOutput;
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString message =
            QStringLiteral("Java 版本检测进程退出异常（exitCode=%1）。\n标准错误：%2")
                .arg(process.exitCode())
                .arg(QString::fromLocal8Bit(standardError).trimmed().isEmpty()
                         ? QStringLiteral("（空）")
                         : QString::fromLocal8Bit(standardError).trimmed());
        if (diagnostic) {
            *diagnostic = message;
        }
        qWarning().noquote() << "[Java probe]" << consoleJava << message;
        return 0;
    }
    static const QRegularExpression versionExpression(
        QStringLiteral("version\\s+\"(\\d+)(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = versionExpression.match(QString::fromLatin1(output));
    if (!match.hasMatch()) {
        const QString message =
            QStringLiteral("无法从 Java 版本输出识别版本号。\n标准错误：%1\n标准输出：%2")
                .arg(QString::fromLocal8Bit(standardError).trimmed().isEmpty()
                         ? QStringLiteral("（空）")
                         : QString::fromLocal8Bit(standardError).trimmed(),
                     QString::fromLocal8Bit(standardOutput).trimmed().isEmpty()
                         ? QStringLiteral("（空）")
                         : QString::fromLocal8Bit(standardOutput).trimmed());
        if (diagnostic) {
            *diagnostic = message;
        }
        qWarning().noquote() << "[Java probe]" << consoleJava << message;
        return 0;
    }
    const int major = match.captured(1).toInt();
    if (major == 1 && !match.captured(2).isEmpty()) {
        const int legacyMajor = match.captured(2).toInt();
        const QMutexLocker locker(&cacheMutex);
        if (versionCache.size() >= 128) {
            versionCache.clear();
        }
        versionCache.insert(cacheKey, CachedVersion{fileSize, modified, legacyMajor});
        return legacyMajor;
    }
    const QMutexLocker locker(&cacheMutex);
    if (versionCache.size() >= 128) {
        versionCache.clear();
    }
    versionCache.insert(cacheKey, CachedVersion{fileSize, modified, major});
    return major;
}

QString nativeArchivesFingerprint(const QStringList &nativeArchives)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayLiteral("mystic-turtle-native-cache-v2\n"));
    for (const QString &archive : nativeArchives) {
        const QFileInfo info(archive);
        hash.addData(archive.toUtf8());
        hash.addData(QByteArray(1, '\0'));
        hash.addData(QByteArray::number(info.size()));
        hash.addData(QByteArray(1, '\0'));
        hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
        hash.addData(QByteArray(1, '\n'));
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool nativeCacheMatches(const QString &nativesDirectory, const QString &fingerprint)
{
    QFile cacheFile(QDir(nativesDirectory).filePath(
        QStringLiteral(".mystic-turtle-native-cache")));
    if (!cacheFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    return QString::fromLatin1(cacheFile.readAll()).trimmed() == fingerprint;
}

bool writeNativeCache(const QString &nativesDirectory, const QString &fingerprint,
                      QString *errorMessage)
{
    QSaveFile cacheFile(QDir(nativesDirectory).filePath(
        QStringLiteral(".mystic-turtle-native-cache")));
    if (!cacheFile.open(QIODevice::WriteOnly)
        || cacheFile.write(fingerprint.toLatin1()) != fingerprint.size()
        || !cacheFile.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法保存 natives 缓存状态：%1")
                                .arg(cacheFile.errorString());
        }
        return false;
    }
    return true;
}

} // namespace

QString GameLauncher::findJavaExecutable(const QString &gameDirectory)
{
    QString runtime = findInRuntime(QDir(gameDirectory).filePath(QStringLiteral("runtime")));
    if (!runtime.isEmpty()) {
        return runtime;
    }
    runtime = findInRuntime(QDir(gameDirectory).filePath(QStringLiteral("../runtime")));
    if (!runtime.isEmpty()) {
        return runtime;
    }
    runtime = findInRuntime(
        QDir(MinecraftPaths::savedGameDirectory()).filePath(QStringLiteral("runtime")));
    if (!runtime.isEmpty()) {
        return runtime;
    }
    runtime = findInRuntime(
        QDir(MinecraftPaths::defaultGameDirectory()).filePath(QStringLiteral("runtime")));
    if (!runtime.isEmpty()) {
        return runtime;
    }

    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString javaHome = environment.value(QStringLiteral("JAVA_HOME"));
    if (!javaHome.trimmed().isEmpty()) {
        const QDir bin(QDir(javaHome).filePath(QStringLiteral("bin")));
        const QString javaw = bin.filePath(QStringLiteral("javaw.exe"));
        if (QFileInfo::exists(javaw)) {
            return javaw;
        }
        const QString java = bin.filePath(QStringLiteral("java.exe"));
        if (QFileInfo::exists(java)) {
            return java;
        }
    }

#if defined(Q_OS_WIN)
    const QString pathJava = QStandardPaths::findExecutable(QStringLiteral("javaw"));
    if (!pathJava.isEmpty()) {
        return pathJava;
    }
    const QString pathJavaConsole = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!pathJavaConsole.isEmpty()) {
        return pathJavaConsole;
    }
#endif

    return QStandardPaths::findExecutable(QStringLiteral("java"));
}

int GameLauncher::javaMajorVersion(const QString &javaExecutable)
{
    return detectJavaMajorVersion(javaExecutable);
}

int GameLauncher::requiredJavaMajor(const QString &gameDirectory,
                                    const QString &versionId)
{
    QVector<QJsonObject> chain;
    if (!readVersionChain(QDir(gameDirectory).absolutePath(), versionId, &chain, nullptr)) {
        return 0;
    }
    for (const QJsonObject &object : std::as_const(chain)) {
        const int major = object.value(QStringLiteral("javaVersion"))
                              .toObject()
                              .value(QStringLiteral("majorVersion"))
                              .toInt();
        if (major > 0) {
            return major;
        }
    }
    if (!chain.isEmpty()) {
        const QString baseVersionId =
            chain.last().value(QStringLiteral("id")).toString().trimmed();
        const int inferred = inferJavaMajorFromVersionId(baseVersionId);
        if (inferred > 0) {
            return inferred;
        }
    }
    return inferJavaMajorFromVersionId(versionId);
}

QString GameLauncher::findCompatibleJava(const QString &gameDirectory,
                                         int requiredMajor,
                                         QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QString bestPath;
    int bestMajor = -1;
    QSet<QString> seen;
    QStringList probeDiagnostics;
    const auto considerCandidate = [&seen, &bestPath, &bestMajor, &probeDiagnostics,
                                    requiredMajor](const QString &path) {
        if (path.trimmed().isEmpty()) {
            return false;
        }
        const QString absolute = QFileInfo(path).absoluteFilePath();
        const QString normalized = QDir::cleanPath(absolute).toCaseFolded();
        if (seen.contains(normalized)) {
            return false;
        }
        seen.insert(normalized);

        QString diagnostic;
        const int major = detectJavaMajorVersion(absolute, &diagnostic);
        if (major <= 0) {
            if (!diagnostic.isEmpty() && probeDiagnostics.size() < 4) {
                probeDiagnostics.append(diagnostic);
            }
            return false;
        }
        const bool compatible = requiredMajor <= 0
            || (requiredMajor <= 8 ? major == requiredMajor
                                   : major >= requiredMajor);
        if (!compatible) {
            if (probeDiagnostics.size() < 4) {
                probeDiagnostics.append(
                    QStringLiteral("Java 版本不兼容：%1（检测到 %2，需要 %3）。")
                        .arg(absolute)
                        .arg(major)
                        .arg(requiredMajor));
            }
            return false;
        }
        if (bestPath.isEmpty()
            || (requiredMajor > 8 && major < bestMajor)
            || (requiredMajor <= 0 && major > bestMajor)) {
            bestMajor = major;
            bestPath = absolute;
        }

        // An exact match is already the best possible result for constrained versions.
        return requiredMajor > 0 && major == requiredMajor;
    };

    const QSettings settings;
    if (considerCandidate(settings.value(QStringLiteral("game/javaPath")).toString())) {
        return javawIfAvailable(bestPath);
    }

    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString javaHome = environment.value(QStringLiteral("JAVA_HOME"));
    if (!javaHome.trimmed().isEmpty()) {
        const QDir bin(QDir(javaHome).filePath(QStringLiteral("bin")));
        if (considerCandidate(bin.filePath(QStringLiteral("javaw.exe")))
            || considerCandidate(bin.filePath(QStringLiteral("java.exe")))) {
            return javawIfAvailable(bestPath);
        }
    }
    if (considerCandidate(QStandardPaths::findExecutable(QStringLiteral("javaw")))
        || considerCandidate(QStandardPaths::findExecutable(QStringLiteral("java")))) {
        return javawIfAvailable(bestPath);
    }

    const QStringList runtimeRoots = {
        QDir(gameDirectory).filePath(QStringLiteral("runtime")),
        QDir(gameDirectory).filePath(QStringLiteral("../runtime")),
        QDir(MinecraftPaths::savedGameDirectory()).filePath(QStringLiteral("runtime")),
        QDir(MinecraftPaths::defaultGameDirectory()).filePath(QStringLiteral("runtime"))
    };
    for (const QString &root : runtimeRoots) {
        if (!QDir(root).exists()) {
            continue;
        }
        QDirIterator iterator(root,
                              QStringList{QStringLiteral("javaw.exe"),
                                          QStringLiteral("java.exe")},
                              QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            if (considerCandidate(iterator.next())) {
                return javawIfAvailable(bestPath);
            }
        }
    }

    if (bestPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = requiredMajor > 0
                ? QStringLiteral("未找到 Java %1 或更高版本。").arg(requiredMajor)
                : QStringLiteral("未找到可用的 Java 运行时。");
            if (!probeDiagnostics.isEmpty()) {
                *errorMessage += QLatin1Char('\n') + probeDiagnostics.join(QLatin1Char('\n'));
            }
        }
        return QString();
    }

    return javawIfAvailable(bestPath);
}

bool GameLauncher::launch(const QString &gameDirectory, const QString &versionId,
                          const PlayerAccount &account, const QString &javaExecutable,
                          const ProgressCallback &progress,
                          qint64 *processId,
                          QString *errorMessage,
                          const QString &gameDataDirectory,
                          const CancelCallback &cancel)
{
    const auto report = [&progress](int percent, const QString &status) {
        if (progress) {
            progress(percent, status);
        }
    };
    const auto cancelled = [&cancel, errorMessage]() {
        if (!cancel || !cancel()) {
            return false;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("已取消启动。");
        }
        return true;
    };

    if (errorMessage) {
        errorMessage->clear();
    }
    if (!account.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先创建并选择一个账户。");
        }
        return false;
    }

    report(5, QStringLiteral("正在读取版本配置…"));
    if (cancelled()) {
        return false;
    }
    const QDir gameDir(QDir(gameDirectory).absolutePath());
    const QDir gameDataDir(gameDataDirectory.trimmed().isEmpty()
                               ? gameDir.absolutePath()
                               : QDir(gameDataDirectory).absolutePath());
    if (!gameDataDir.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("游戏数据目录不存在：%1")
                                .arg(gameDataDir.absolutePath());
        }
        return false;
    }
    QVector<QJsonObject> chain;
    if (!readVersionChain(gameDir.absolutePath(), versionId, &chain, errorMessage)) {
        return false;
    }
    report(18, QStringLiteral("正在解析依赖库…"));
    if (cancelled()) {
        return false;
    }

    QJsonArray libraries;
    QJsonArray gameArguments;
    QJsonArray jvmArguments;
    QJsonObject assetIndex;
    QJsonObject loggingFile;
    QString mainClass;
    QString assetsId;
    QString versionType;
    QString legacyArguments;
    QString rootVersionId;
    int requiredJava = 0;

    for (int i = chain.size() - 1; i >= 0; --i) {
        const QJsonObject &object = chain.at(i);
        const QJsonArray libs = object.value(QStringLiteral("libraries")).toArray();
        for (const QJsonValue &library : libs) {
            libraries.append(library);
        }

        const QJsonObject arguments = object.value(QStringLiteral("arguments")).toObject();
        const QJsonArray game = arguments.value(QStringLiteral("game")).toArray();
        const QJsonArray jvm = arguments.value(QStringLiteral("jvm")).toArray();
        for (const QJsonValue &value : game) {
            gameArguments.append(value);
        }
        for (const QJsonValue &value : jvm) {
            jvmArguments.append(value);
        }
    }

    if (!chain.isEmpty()) {
        rootVersionId = chain.last().value(QStringLiteral("id")).toString().trimmed();
    }
    if (rootVersionId.isEmpty()) {
        rootVersionId = versionId;
    }
    if (requiredJava <= 0) {
        requiredJava = inferJavaMajorFromVersionId(rootVersionId);
    }

    for (const QJsonObject &object : std::as_const(chain)) {
        if (mainClass.isEmpty()) {
            mainClass = object.value(QStringLiteral("mainClass")).toString().trimmed();
        }
        if (versionType.isEmpty()) {
            versionType = object.value(QStringLiteral("type")).toString().trimmed();
        }
        if (legacyArguments.isEmpty()) {
            legacyArguments =
                object.value(QStringLiteral("minecraftArguments")).toString();
        }
        if (assetIndex.isEmpty()) {
            assetIndex = object.value(QStringLiteral("assetIndex")).toObject();
        }
        if (loggingFile.isEmpty()) {
            loggingFile = object.value(QStringLiteral("logging")).toObject()
                              .value(QStringLiteral("client")).toObject()
                              .value(QStringLiteral("file")).toObject();
        }
        if (assetsId.isEmpty()) {
            assetsId = object.value(QStringLiteral("assets")).toString().trimmed();
        }
        if (requiredJava <= 0) {
            requiredJava = object.value(QStringLiteral("javaVersion"))
                               .toObject()
                               .value(QStringLiteral("majorVersion"))
                               .toInt();
        }
    }
    if (rootVersionId.isEmpty()) {
        rootVersionId = versionId;
    }
    if (versionType.isEmpty()) {
        versionType = QStringLiteral("release");
    }
    if (mainClass.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本 JSON 缺少主类。");
        }
        return false;
    }

    if (assetsId.isEmpty()) {
        assetsId = assetIndex.value(QStringLiteral("id")).toString().trimmed();
    }
    const QString assetsRoot = gameDir.filePath(QStringLiteral("assets"));
    const QString assetIndexPath = QDir(assetsRoot)
                                       .filePath(QStringLiteral("indexes/%1.json")
                                                     .arg(assetsId));

    QString java = javaExecutable.trimmed();
    if (java.isEmpty() || !QFileInfo::exists(java)) {
        const QSettings settings;
        const QString configured =
            settings.value(QStringLiteral("game/javaPath")).toString().trimmed();
        if (!configured.isEmpty() && QFileInfo::exists(configured)) {
            java = configured;
        } else {
            java = GameLauncher::findJavaExecutable(gameDir.absolutePath());
        }
    }
    if (java.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到 Java 运行时。请安装 Java 或在设置中配置 Java 路径。");
        }
        return false;
    }

    const int detectedJava = detectJavaMajorVersion(java);
    const bool javaCompatible = requiredJava <= 0
        || (requiredJava <= 8 ? detectedJava == requiredJava
                              : detectedJava >= requiredJava);
    if (!javaCompatible) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("该版本需要 Java %1，当前找到的是 Java %2。请安装合适的 Java 运行时。")
                    .arg(requiredJava)
                    .arg(detectedJava);
        }
        return false;
    }

    QJsonObject clientDownload;
    QString clientVersionId = rootVersionId;
    for (const QJsonObject &object : std::as_const(chain)) {
        const QJsonObject candidate =
            object.value(QStringLiteral("downloads")).toObject()
                .value(QStringLiteral("client")).toObject();
        if (!candidate.isEmpty()) {
            clientDownload = candidate;
            clientVersionId = object.value(QStringLiteral("id")).toString().trimmed();
            break;
        }
    }

    QString clientJar = gameDir.filePath(
        QStringLiteral("versions/%1/%1.jar").arg(versionId));
    const QString rootClientJar = gameDir.filePath(
        QStringLiteral("versions/%1/%1.jar").arg(rootVersionId));
    if (!QFileInfo::exists(clientJar) && QFileInfo::exists(rootClientJar)) {
        clientJar = rootClientJar;
    } else if (!QFileInfo::exists(clientJar) && !clientVersionId.isEmpty()) {
        clientJar = gameDir.filePath(
            QStringLiteral("versions/%1/%1.jar").arg(clientVersionId));
    }

    QList<LibraryDownload> requiredGameFiles;
    if (!clientDownload.isEmpty()) {
        requiredGameFiles.append(LibraryDownload{
            QDir(gameDir.absolutePath()).relativeFilePath(clientJar),
            clientDownload.value(QStringLiteral("url")).toString().trimmed(),
            clientDownload.value(QStringLiteral("sha1")).toString().trimmed().toLower(),
            static_cast<qint64>(clientDownload.value(QStringLiteral("size")).toDouble(-1.0))});
    }
    if (!assetIndex.isEmpty() && !assetsId.isEmpty()) {
        requiredGameFiles.append(LibraryDownload{
            QStringLiteral("assets/indexes/%1.json").arg(assetsId),
            assetIndex.value(QStringLiteral("url")).toString().trimmed(),
            assetIndex.value(QStringLiteral("sha1")).toString().trimmed().toLower(),
            static_cast<qint64>(assetIndex.value(QStringLiteral("size")).toDouble(-1.0))});
    }
    const QString loggingFileId =
        loggingFile.value(QStringLiteral("id")).toString().trimmed();
    if (!loggingFile.isEmpty() && !loggingFileId.isEmpty()) {
        requiredGameFiles.append(LibraryDownload{
            QStringLiteral("assets/log_configs/%1").arg(loggingFileId),
            loggingFile.value(QStringLiteral("url")).toString().trimmed(),
            loggingFile.value(QStringLiteral("sha1")).toString().trimmed().toLower(),
            static_cast<qint64>(loggingFile.value(QStringLiteral("size")).toDouble(-1.0))});
    }
    report(20, QStringLiteral("正在检查客户端和资源索引…"));
    if (!ensureGameFiles(gameDir.absolutePath(), requiredGameFiles,
                         progress, cancel, errorMessage, 20, 10,
                         QStringLiteral("正在补全客户端和资源索引…"))) {
        return false;
    }
    if (cancelled()) {
        return false;
    }

    if (!assetIndex.isEmpty() && !assetsId.isEmpty()) {
        QFile indexFile(assetIndexPath);
        if (!indexFile.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取资源索引：%1")
                                    .arg(indexFile.errorString());
            }
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument indexDocument =
            QJsonDocument::fromJson(indexFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !indexDocument.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("资源索引解析失败：%1")
                                    .arg(parseError.errorString());
            }
            return false;
        }

        const QJsonObject objects =
            indexDocument.object().value(QStringLiteral("objects")).toObject();
        QList<LibraryDownload> requiredAssetFiles;
        requiredAssetFiles.reserve(objects.size());
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
            const QJsonObject object = it.value().toObject();
            const QString hash = object.value(QStringLiteral("hash"))
                                     .toString().trimmed().toLower();
            if (!QRegularExpression(QStringLiteral("^[0-9a-f]{40}$"))
                     .match(hash).hasMatch()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("资源索引中的文件 SHA-1 无效：%1")
                                        .arg(it.key());
                }
                return false;
            }
            const qint64 size =
                static_cast<qint64>(object.value(QStringLiteral("size")).toDouble(-1.0));
            const QString relativePath =
                QStringLiteral("assets/objects/%1/%2").arg(hash.left(2), hash);
            requiredAssetFiles.append(LibraryDownload{
                relativePath,
                QStringLiteral("https://resources.download.minecraft.net/%1/%2")
                    .arg(hash.left(2), hash),
                hash,
                size,
                size <= 0});
        }
        report(30, QStringLiteral("正在检查游戏资源…"));
        if (!ensureGameFiles(gameDir.absolutePath(), requiredAssetFiles,
                             progress, cancel, errorMessage, 30, 10,
                             QStringLiteral("正在补全游戏资源…"))) {
            return false;
        }
        if (cancelled()) {
            return false;
        }
    }

    report(45, QStringLiteral("正在准备本地依赖…"));
    if (cancelled()) {
        return false;
    }
    const QString nativesDirectory =
        gameDir.filePath(QStringLiteral("versions/%1/natives").arg(versionId));

    QStringList classpathModuleOrder;
    QHash<QString, QString> classpathByModule;
    QStringList nativeArchives;
    QSet<QString> nativeArchivesSeen;
    QList<LibraryDownload> requiredLibraryFiles;
    QSet<QString> requiredLibraryPaths;
    for (const QJsonValue &value : libraries) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject library = value.toObject();
        if (!rulesAllow(library.value(QStringLiteral("rules")).toArray())) {
            continue;
        }

        const QString name = library.value(QStringLiteral("name")).toString();
        const QJsonObject downloads = library.value(QStringLiteral("downloads")).toObject();
        const QJsonObject artifactObject =
            downloads.value(QStringLiteral("artifact")).toObject();
        QString artifactPath =
            artifactObject.value(QStringLiteral("path")).toString().trimmed();
        if (artifactPath.isEmpty() && (downloads.isEmpty() || !artifactObject.isEmpty())) {
            // Legacy version JSONs omit downloads and rely on Maven coordinates.
            artifactPath = mavenPath(name);
        }
        artifactPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (artifactPath.startsWith(QStringLiteral("libraries/"))) {
            artifactPath.remove(0, 10);
        }

        const auto addLibraryDownload = [&](const QString &path,
                                            const QJsonObject &downloadObject) {
            QString normalizedPath = path;
            normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (normalizedPath.startsWith(QStringLiteral("libraries/"))) {
                normalizedPath.remove(0, 10);
            }
            if (normalizedPath.isEmpty() || requiredLibraryPaths.contains(normalizedPath)) {
                return;
            }
            requiredLibraryPaths.insert(normalizedPath);

            QString url = downloadObject.value(QStringLiteral("url")).toString().trimmed();
            if (url.isEmpty()) {
                QString baseUrl =
                    library.value(QStringLiteral("url")).toString().trimmed();
                if (baseUrl.isEmpty()) {
                    baseUrl = QStringLiteral("https://libraries.minecraft.net/");
                }
                if (!baseUrl.endsWith(QLatin1Char('/'))) {
                    baseUrl += QLatin1Char('/');
                }
                url = baseUrl + normalizedPath;
            }

            const QJsonValue sizeValue = downloadObject.value(QStringLiteral("size"));
            const qint64 expectedSize = sizeValue.isDouble()
                ? static_cast<qint64>(sizeValue.toDouble(-1.0))
                : -1;
            requiredLibraryFiles.append(LibraryDownload{
                QStringLiteral("libraries/%1").arg(normalizedPath),
                url,
                downloadObject.value(QStringLiteral("sha1")).toString().trimmed().toLower(),
                expectedSize});
        };
        const QString nativeHint = (name + QLatin1Char(' ') + artifactPath).toLower();
        if (currentArchName() != QStringLiteral("arm64")
            && nativeHint.contains(QStringLiteral("natives-windows-arm64"))) {
            continue;
        }
        if (currentArchName() != QStringLiteral("x86")
            && nativeHint.contains(QStringLiteral("natives-windows-x86"))) {
            continue;
        }

        addLibraryDownload(artifactPath, artifactObject);

        QString nativePath = nativeJarRelativePath(library);
        nativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (nativePath.startsWith(QStringLiteral("libraries/"))) {
            nativePath.remove(0, 10);
        }
        if (!nativePath.isEmpty() && nativePath != artifactPath) {
            const QString classifier = expandedNativeClassifier(
                library.value(QStringLiteral("natives")).toObject()
                    .value(currentOsName()).toString().trimmed());
            const QJsonObject classifierObject =
                downloads.value(QStringLiteral("classifiers")).toObject()
                    .value(classifier).toObject();
            addLibraryDownload(nativePath, classifierObject);
        }
        if (!artifactPath.isEmpty()) {
            const QString absolute = gameDir.filePath(
                QStringLiteral("libraries/%1").arg(artifactPath));
            if (nativePath.isEmpty() || nativePath != artifactPath) {
                QString moduleKey = mavenModuleKey(name);
                if (moduleKey.isEmpty()) {
                    moduleKey = absolute;
                }
                if (!classpathByModule.contains(moduleKey)) {
                    classpathModuleOrder.append(moduleKey);
                }
                classpathByModule.insert(moduleKey, absolute);
            }
        }

        if (!nativePath.isEmpty() && !nativeArchivesSeen.contains(nativePath)) {
            const QString nativeJar = gameDir.filePath(
                QStringLiteral("libraries/%1").arg(nativePath));
            nativeArchivesSeen.insert(nativePath);
            nativeArchives.append(nativeJar);
        }
    }

    report(45, QStringLiteral("正在检查游戏依赖库…"));
    if (!ensureGameFiles(gameDir.absolutePath(), requiredLibraryFiles,
                         progress, cancel, errorMessage, 45, 8,
                         QStringLiteral("正在补全游戏依赖库…"))) {
        return false;
    }
    if (cancelled()) {
        return false;
    }

    QStringList classpath;
    QSet<QString> classpathSeen;
    for (const QString &moduleKey : std::as_const(classpathModuleOrder)) {
        const QString &path = classpathByModule[moduleKey];
        if (!QFileInfo::exists(path)) {
            const QString relativePath =
                QDir(gameDir.filePath(QStringLiteral("libraries")))
                    .relativeFilePath(path);
            if (errorMessage) {
                *errorMessage = QStringLiteral("缺少依赖库：%1。请重新下载该版本。")
                                    .arg(relativePath);
            }
            return false;
        }
        if (!classpathSeen.contains(path)) {
            classpathSeen.insert(path);
            classpath.append(path);
        }
    }

    const QString nativeFingerprint = nativeArchivesFingerprint(nativeArchives);
    const bool nativeCacheValid =
        QDir(nativesDirectory).exists()
        && nativeCacheMatches(nativesDirectory, nativeFingerprint);
    if (!nativeCacheValid) {
        report(55, QStringLiteral("正在解压 natives…"));
        if (cancelled()) {
            return false;
        }
        if (QDir(nativesDirectory).exists()
            && !QDir(nativesDirectory).removeRecursively()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法清理旧 natives 目录：%1")
                                    .arg(nativesDirectory);
            }
            return false;
        }
        if (!QDir().mkpath(nativesDirectory)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建 natives 目录：%1")
                                    .arg(nativesDirectory);
            }
            return false;
        }
        for (const QString &nativeJar : std::as_const(nativeArchives)) {
            if (!extractZipArchive(nativeJar, nativesDirectory, errorMessage)) {
                return false;
            }
        }
        if (currentOsName() == QStringLiteral("windows")
            && !flattenWindowsNativeLibraries(nativesDirectory, errorMessage)) {
            return false;
        }
        if (!writeNativeCache(nativesDirectory, nativeFingerprint, errorMessage)) {
            return false;
        }
    } else {
        report(65, QStringLiteral("本地依赖已就绪…"));
    }

    if (!QFileInfo::exists(clientJar)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("缺少客户端 JAR：%1。请重新下载该版本。")
                                .arg(clientJar);
        }
        return false;
    }
    if (!classpathSeen.contains(clientJar)) {
        classpath.append(clientJar);
    }

    const QString classPath = classpath.join(QLatin1Char(';'));
    report(72, QStringLiteral("正在生成启动参数…"));
    if (cancelled()) {
        return false;
    }
    const QString uuid = offlineUuid(account.username);
    QHash<QString, QString> values;
    values.insert(QStringLiteral("auth_player_name"), account.username);
    values.insert(QStringLiteral("auth_uuid"), uuid);
    values.insert(QStringLiteral("auth_access_token"), QStringLiteral("0"));
    values.insert(QStringLiteral("auth_session"), QStringLiteral("0"));
    values.insert(QStringLiteral("user_type"), QStringLiteral("legacy"));
    values.insert(QStringLiteral("user_properties"), QStringLiteral("{}"));
    values.insert(QStringLiteral("clientid"), QStringLiteral("mysticturtle"));
    values.insert(QStringLiteral("auth_xuid"), QString());
    values.insert(QStringLiteral("version_name"), versionId);
    values.insert(QStringLiteral("version_type"), versionType);
    values.insert(QStringLiteral("game_directory"),
                  QDir::toNativeSeparators(gameDataDir.absolutePath()));
    values.insert(QStringLiteral("assets_root"), QDir::toNativeSeparators(assetsRoot));
    values.insert(QStringLiteral("assets_index_name"), assetsId);
    values.insert(QStringLiteral("game_assets"), QDir::toNativeSeparators(assetsRoot));
    values.insert(QStringLiteral("natives_directory"), QDir::toNativeSeparators(nativesDirectory));
    values.insert(QStringLiteral("library_directory"),
                  QDir::toNativeSeparators(gameDir.filePath(QStringLiteral("libraries"))));
    values.insert(QStringLiteral("classpath"), QDir::toNativeSeparators(classPath));
    values.insert(QStringLiteral("classpath_separator"), QStringLiteral(";"));
    values.insert(QStringLiteral("launcher_name"), QStringLiteral("MysticTurtle"));
    values.insert(QStringLiteral("launcher_version"), QStringLiteral("1.0.0"));
    values.insert(QStringLiteral("resolution_width"), QStringLiteral("854"));
    values.insert(QStringLiteral("resolution_height"), QStringLiteral("480"));

    QStringList jvmArgs;
    appendArgumentArray(jvmArguments, &jvmArgs, values);

    QStringList filteredJvmArgs;
    filteredJvmArgs.reserve(jvmArgs.size());
    bool hasClassPath = false;
    for (const QString &argument : std::as_const(jvmArgs)) {
        if (argument.startsWith(QStringLiteral("-Djava.library.path="))
            || argument.startsWith(QStringLiteral("-Xmx"))
            || argument.startsWith(QStringLiteral("-Xms"))) {
            continue;
        }
        filteredJvmArgs.append(argument);
        if (argument == QStringLiteral("-cp") || argument == QStringLiteral("--class-path")) {
            hasClassPath = true;
        }
    }
    jvmArgs = filteredJvmArgs;
    if (!hasClassPath) {
        jvmArgs.append(QStringLiteral("-cp"));
        jvmArgs.append(QDir::toNativeSeparators(classPath));
    }
    jvmArgs.append(QStringLiteral("-Djava.library.path=%1")
                       .arg(QDir::toNativeSeparators(nativesDirectory)));

    const QSettings settings;
    const int memoryGb = qBound(
        2, settings.value(QStringLiteral("game/memoryGb"), 8).toInt(), 16);
    jvmArgs.append(QStringLiteral("-Xmx%1G").arg(memoryGb));

    QStringList gameArgs;
    if (!gameArguments.isEmpty()) {
        appendArgumentArray(gameArguments, &gameArgs, values);
    } else if (!legacyArguments.isEmpty()) {
        gameArgs = QProcess::splitCommand(substitute(legacyArguments, values));
    }

    if (!assetsId.isEmpty() && !QFileInfo::exists(assetIndexPath)) {
        // Keep going for legacy versions with no downloadable index. Modern
        // installations normally have this file already.
    }

    QStringList arguments = jvmArgs;
    arguments.append(mainClass);
    arguments.append(gameArgs);

    QString launchJava = javaVersionProbePath(java);
    if (!QFileInfo::exists(launchJava)) {
        launchJava = java;
    }

    const QString launchLogsDirectory =
        gameDataDir.filePath(QStringLiteral("logs"));
    if (!QDir().mkpath(launchLogsDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建启动日志目录：%1")
                                .arg(launchLogsDirectory);
        }
        return false;
    }
    const QString stdoutLogPath =
        QDir(launchLogsDirectory).filePath(QStringLiteral("mysticturtle-launcher.stdout.log"));
    const QString stderrLogPath =
        QDir(launchLogsDirectory).filePath(QStringLiteral("mysticturtle-launcher.stderr.log"));

    report(88, QStringLiteral("正在准备首次启动设置…"));
    if (cancelled()) {
        return false;
    }
    QString optionsError;
    if (!prepareFirstLaunchOptions(gameDataDir.absolutePath(), &optionsError)) {
        qWarning() << "Unable to initialize optional Minecraft options:"
                   << optionsError;
    }

    report(92, QStringLiteral("正在启动 Java 进程…"));
    if (cancelled()) {
        return false;
    }
    qint64 launchedPid = 0;
    QProcess process;
    process.setProgram(launchJava);
    process.setArguments(arguments);
    process.setWorkingDirectory(gameDataDir.absolutePath());
    process.setStandardOutputFile(stdoutLogPath);
    process.setStandardErrorFile(stderrLogPath);
    if (!process.startDetached(&launchedPid)) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("Java 进程启动失败：%1\nJava：%2\n错误日志：%3")
                    .arg(process.errorString(), launchJava, stderrLogPath);
        }
        return false;
    }
    if (processId) {
        *processId = launchedPid;
    }

    report(100, QStringLiteral("游戏已启动"));
    return true;
}
