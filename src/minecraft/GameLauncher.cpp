/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "GameLauncher.h"

#include "MinecraftPaths.h"
#include "account/AccountManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
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
    const QString classifier = natives.value(currentOsName()).toString().trimmed();
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

int detectJavaMajorVersion(const QString &javaExecutable)
{
    QString consoleJava = javaExecutable;
    const QFileInfo info(javaExecutable);
    if (info.fileName().compare(QStringLiteral("javaw.exe"), Qt::CaseInsensitive) == 0) {
        const QString sibling = info.absoluteDir().filePath(QStringLiteral("java.exe"));
        if (QFileInfo::exists(sibling)) {
            consoleJava = sibling;
        }
    }

    QProcess process;
    process.start(consoleJava, QStringList{QStringLiteral("-version")});
    if (!process.waitForStarted(2000) || !process.waitForFinished(3500)) {
        process.kill();
        return 0;
    }

    const QByteArray output = process.readAllStandardError() + process.readAllStandardOutput();
    static const QRegularExpression versionExpression(
        QStringLiteral("version\\s+\"(\\d+)(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = versionExpression.match(QString::fromLatin1(output));
    if (!match.hasMatch()) {
        return 0;
    }
    const int major = match.captured(1).toInt();
    if (major == 1 && !match.captured(2).isEmpty()) {
        return match.captured(2).toInt();
    }
    return major;
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
    return 0;
}

QString GameLauncher::findCompatibleJava(const QString &gameDirectory,
                                         int requiredMajor,
                                         QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QStringList candidates;
    QSet<QString> seen;
    const auto addCandidate = [&candidates, &seen](const QString &path) {
        if (path.trimmed().isEmpty()) {
            return;
        }
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (QFileInfo::exists(absolute) && !seen.contains(absolute)) {
            seen.insert(absolute);
            candidates.append(absolute);
        }
    };

    const QSettings settings;
    addCandidate(settings.value(QStringLiteral("game/javaPath")).toString());

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
            addCandidate(iterator.next());
        }
    }

    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString javaHome = environment.value(QStringLiteral("JAVA_HOME"));
    if (!javaHome.trimmed().isEmpty()) {
        const QDir bin(QDir(javaHome).filePath(QStringLiteral("bin")));
        addCandidate(bin.filePath(QStringLiteral("javaw.exe")));
        addCandidate(bin.filePath(QStringLiteral("java.exe")));
    }
    addCandidate(QStandardPaths::findExecutable(QStringLiteral("javaw")));
    addCandidate(QStandardPaths::findExecutable(QStringLiteral("java")));
    addCandidate(findJavaExecutable(gameDirectory));

    QString bestPath;
    int bestMajor = -1;
    for (const QString &candidate : std::as_const(candidates)) {
        const int major = detectJavaMajorVersion(candidate);
        if (major <= 0) {
            continue;
        }
        bool compatible = true;
        if (requiredMajor > 0) {
            if (requiredMajor <= 8) {
                compatible = major == requiredMajor;
            } else {
                compatible = major >= requiredMajor;
            }
        }
        if (!compatible) {
            continue;
        }
        if (bestPath.isEmpty()
            || (requiredMajor > 8 && major < bestMajor)
            || (requiredMajor <= 0 && major > bestMajor)) {
            bestMajor = major;
            bestPath = candidate;
        }
    }

    if (bestPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = requiredMajor > 0
                ? QStringLiteral("未找到 Java %1 或更高版本。").arg(requiredMajor)
                : QStringLiteral("未找到可用的 Java 运行时。");
        }
        return QString();
    }

    const QFileInfo info(bestPath);
    if (info.fileName().compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0) {
        const QString javaw = info.absoluteDir().filePath(QStringLiteral("javaw.exe"));
        if (QFileInfo::exists(javaw)) {
            return javaw;
        }
    }
    return bestPath;
}

bool GameLauncher::launch(const QString &gameDirectory, const QString &versionId,
                          const PlayerAccount &account, const QString &javaExecutable,
                          const ProgressCallback &progress,
                          qint64 *processId,
                          QString *errorMessage)
{
    const auto report = [&progress](int percent, const QString &status) {
        if (progress) {
            progress(percent, status);
        }
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
    const QDir gameDir(QDir(gameDirectory).absolutePath());
    QVector<QJsonObject> chain;
    if (!readVersionChain(gameDir.absolutePath(), versionId, &chain, errorMessage)) {
        return false;
    }
    report(18, QStringLiteral("正在解析依赖库…"));

    QJsonArray libraries;
    QJsonArray gameArguments;
    QJsonArray jvmArguments;
    QJsonObject assetIndex;
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

    report(45, QStringLiteral("正在准备本地依赖…"));
    const QString nativesDirectory =
        gameDir.filePath(QStringLiteral("versions/%1/natives").arg(versionId));
    QDir(nativesDirectory).removeRecursively();
    if (!QDir().mkpath(nativesDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建 natives 目录：%1").arg(nativesDirectory);
        }
        return false;
    }

    QStringList classpath;
    QSet<QString> classpathSeen;
    QSet<QString> extractedNatives;
    report(55, QStringLiteral("正在解压 natives…"));
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
        QString artifactPath = downloads.value(QStringLiteral("artifact")).toObject()
                                   .value(QStringLiteral("path")).toString();
        if (artifactPath.isEmpty() && downloads.isEmpty()) {
            // Legacy version JSONs omit downloads and rely on Maven coordinates.
            artifactPath = mavenPath(name);
        }

        const QString nativeHint = (name + QLatin1Char(' ') + artifactPath).toLower();
        if (currentArchName() != QStringLiteral("arm64")
            && nativeHint.contains(QStringLiteral("natives-windows-arm64"))) {
            continue;
        }
        if (currentArchName() != QStringLiteral("x86")
            && nativeHint.contains(QStringLiteral("natives-windows-x86"))) {
            continue;
        }

        if (!artifactPath.isEmpty()) {
            const QString absolute = gameDir.filePath(
                QStringLiteral("libraries/%1").arg(artifactPath));
            if (!QFileInfo::exists(absolute)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("缺少依赖库：%1。请重新下载该版本。")
                                        .arg(artifactPath);
                }
                return false;
            }
            if (!classpathSeen.contains(absolute)) {
                classpathSeen.insert(absolute);
                classpath.append(absolute);
            }
        }

        const QString nativePath = nativeJarRelativePath(library);
        if (!nativePath.isEmpty() && !extractedNatives.contains(nativePath)) {
            const QString nativeJar = gameDir.filePath(
                QStringLiteral("libraries/%1").arg(nativePath));
            if (!QFileInfo::exists(nativeJar)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("缺少本地依赖库：%1。请重新下载该版本。")
                                        .arg(nativePath);
                }
                return false;
            }

            if (!extractZipArchive(nativeJar, nativesDirectory, errorMessage)) {
                return false;
            }
            extractedNatives.insert(nativePath);
        }
    }

    QString clientJar = gameDir.filePath(
        QStringLiteral("versions/%1/%1.jar").arg(versionId));
    if (!QFileInfo::exists(clientJar)) {
        clientJar = gameDir.filePath(
            QStringLiteral("versions/%1/%1.jar").arg(rootVersionId));
    }
    if (!QFileInfo::exists(clientJar)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("缺少客户端 JAR：%1。请重新下载该版本。")
                                .arg(clientJar);
        }
        return false;
    }
    if (!classpathSeen.contains(clientJar)) {
        classpathSeen.insert(clientJar);
        classpath.append(clientJar);
    }

    const QString classPath = classpath.join(QLatin1Char(';'));
    report(72, QStringLiteral("正在生成启动参数…"));
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
    values.insert(QStringLiteral("game_directory"), QDir::toNativeSeparators(gameDir.absolutePath()));
    values.insert(QStringLiteral("assets_root"), QDir::toNativeSeparators(assetsRoot));
    values.insert(QStringLiteral("assets_index_name"), assetsId);
    values.insert(QStringLiteral("game_assets"), QDir::toNativeSeparators(assetsRoot));
    values.insert(QStringLiteral("natives_directory"), QDir::toNativeSeparators(nativesDirectory));
    values.insert(QStringLiteral("library_directory"),
                  QDir::toNativeSeparators(gameDir.filePath(QStringLiteral("libraries"))));
    values.insert(QStringLiteral("classpath"), QDir::toNativeSeparators(classPath));
    values.insert(QStringLiteral("classpath_separator"), QStringLiteral(";"));
    values.insert(QStringLiteral("launcher_name"), QStringLiteral("MysticTurtle"));
    values.insert(QStringLiteral("launcher_version"), QStringLiteral("0.1.0"));
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
        2, settings.value(QStringLiteral("game/memoryGb"), 6).toInt(), 16);
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

    report(92, QStringLiteral("正在启动 Java 进程…"));
    qint64 launchedPid = 0;
    if (!QProcess::startDetached(java, arguments, gameDir.absolutePath(), &launchedPid)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Java 进程启动失败。请检查 Java 是否可用。");
        }
        return false;
    }
    if (processId) {
        *processId = launchedPid;
    }

    report(100, QStringLiteral("游戏已启动"));
    return true;
}
