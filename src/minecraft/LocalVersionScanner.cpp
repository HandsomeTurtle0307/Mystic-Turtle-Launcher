/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "LocalVersionScanner.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

struct LoaderInfo {
    QString name;
    QString version;
    QColor color;
};

QString mavenVersion(const QString &name)
{
    const QStringList parts = name.split(QLatin1Char(':'));
    return parts.size() >= 3 ? parts.at(2) : QString();
}

LoaderInfo loaderFromJson(const QJsonObject &root, const QString &versionId,
                          const QString &minecraftVersion)
{
    const QJsonArray libraries = root.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &value : libraries) {
        if (!value.isObject()) {
            continue;
        }
        const QString name =
            value.toObject().value(QStringLiteral("name")).toString();
        const QString lower = name.toLower();
        QString loaderVersion = mavenVersion(name);

        if (lower.contains(QStringLiteral("fabric-loader"))) {
            return {QStringLiteral("Fabric"), loaderVersion,
                    QColor(QStringLiteral("#89A8FF"))};
        }
        if (lower.contains(QStringLiteral("neoforge"))) {
            return {QStringLiteral("NeoForge"), loaderVersion,
                    QColor(QStringLiteral("#B7A0FF"))};
        }
        if (lower.contains(QStringLiteral("forge"))) {
            if (!minecraftVersion.isEmpty()
                && loaderVersion.startsWith(minecraftVersion + QLatin1Char('-'))) {
                loaderVersion = loaderVersion.mid(minecraftVersion.size() + 1);
            }
            return {QStringLiteral("Forge"), loaderVersion,
                    QColor(QStringLiteral("#FFC66D"))};
        }
    }

    const QString lowerId = versionId.toLower();
    if (lowerId.contains(QStringLiteral("neoforge"))) {
        return {QStringLiteral("NeoForge"), QString(), QColor(QStringLiteral("#7C6CF6"))};
    }
    if (lowerId.contains(QStringLiteral("fabric"))) {
        return {QStringLiteral("Fabric"), QString(), QColor(QStringLiteral("#2F80ED"))};
    }
    if (lowerId.contains(QStringLiteral("forge"))) {
        return {QStringLiteral("Forge"), QString(), QColor(QStringLiteral("#D98A13"))};
    }
    return {QStringLiteral("原版"), QStringLiteral("官方"),
            QColor(QStringLiteral("#1F9D6B"))};
}

QVector<ModInfo> scanMods(const QString &modsDirectory)
{
    QVector<ModInfo> mods;
    QDir directory(modsDirectory);
    if (!directory.exists()) {
        return mods;
    }

    QFileInfoList jarFiles;
    QDirIterator iterator(modsDirectory, QDir::Files | QDir::Readable,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QFileInfo file(iterator.next());
        const QString fileName = file.fileName();
        if (fileName.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)
            || fileName.endsWith(QStringLiteral(".jar.disabled"), Qt::CaseInsensitive)) {
            jarFiles.append(file);
        }
    }
    std::sort(jarFiles.begin(), jarFiles.end(),
              [](const QFileInfo &left, const QFileInfo &right) {
                  return left.fileName().compare(right.fileName(), Qt::CaseInsensitive) < 0;
              });

    for (const QFileInfo &file : jarFiles) {
        const QString fileName = file.fileName();
        const bool enabled = !fileName.endsWith(QStringLiteral(".disabled"),
                                                Qt::CaseInsensitive);
        QString displayName = fileName;
        if (!enabled) {
            displayName.chop(9); // ".disabled"
        }
        if (displayName.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
            displayName.chop(4);
        }

        ModInfo mod;
        mod.name = displayName;
        mod.description = enabled
            ? QStringLiteral("位于 mods 目录的本地模组。")
            : QStringLiteral("位于 mods 目录的本地模组（已停用）。");
        mod.category = QStringLiteral("本地");
        mod.enabled = enabled;
        mod.filePath = file.absoluteFilePath();
        mods.append(mod);
    }
    return mods;
}

QVector<ModInfo> versionMods(const QDir &versionDirectory,
                             const QVector<ModInfo> &globalMods,
                             bool isolateVersions)
{
    const QString isolatedMods = versionDirectory.filePath(QStringLiteral("mods"));
    if (QDir(isolatedMods).exists()) {
        return scanMods(isolatedMods);
    }

    const QString nestedMods =
        versionDirectory.filePath(QStringLiteral(".minecraft/mods"));
    if (QDir(nestedMods).exists()) {
        return scanMods(nestedMods);
    }

    return isolateVersions ? QVector<ModInfo>() : globalMods;
}

QString formatLastModified(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QStringLiteral("未知");
    }

    const QDateTime local = dateTime.toLocalTime();
    const QDate today = QDate::currentDate();
    if (local.date() == today) {
        return QStringLiteral("今天 %1").arg(local.time().toString(QStringLiteral("HH:mm")));
    }
    if (local.date() == today.addDays(-1)) {
        return QStringLiteral("昨天 %1").arg(local.time().toString(QStringLiteral("HH:mm")));
    }
    if (local.date().year() == today.year()) {
        return QStringLiteral("%1 月 %2 日").arg(local.date().month()).arg(local.date().day());
    }
    return QStringLiteral("%1 年 %2 月 %3 日")
        .arg(local.date().year())
        .arg(local.date().month())
        .arg(local.date().day());
}

QString gameDataDirectory(const QDir &versionDirectory,
                          const QString &defaultDirectory,
                          bool isolateVersions)
{
    const QString isolatedMods =
        versionDirectory.filePath(QStringLiteral("mods"));
    if (QDir(isolatedMods).exists()) {
        return versionDirectory.absolutePath();
    }

    const QString nestedGameDirectory =
        versionDirectory.filePath(QStringLiteral(".minecraft"));
    if (QDir(QDir(nestedGameDirectory).filePath(QStringLiteral("mods"))).exists()) {
        return nestedGameDirectory;
    }

    return isolateVersions ? versionDirectory.absolutePath() : defaultDirectory;
}

struct VersionFileCandidate {
    QFileInfo jsonFile;
};

} // namespace

namespace LocalVersionScanner {

Result scan(const QString &gameDirectory)
{
    Result result;
    result.gameDirectory = QDir(gameDirectory).absolutePath();
    const bool isolateVersions = true;

    const QVector<ModInfo> globalMods =
        scanMods(QDir(result.gameDirectory).filePath(QStringLiteral("mods")));

    const QDir versionsDir(QDir(result.gameDirectory).filePath(QStringLiteral("versions")));
    if (!versionsDir.exists()) {
        result.mods = globalMods;
        result.error = QStringLiteral("当前目录下没有 versions 文件夹。");
        return result;
    }

    QVector<VersionFileCandidate> candidates;
    const QFileInfoList versionFolders = versionsDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &folder : versionFolders) {
        QFileInfo jsonFile(QDir(folder.absoluteFilePath())
                               .filePath(folder.fileName() + QStringLiteral(".json")));
        if (!jsonFile.isFile()) {
            const QFileInfoList jsonFiles = QDir(folder.absoluteFilePath())
                .entryInfoList(QStringList{QStringLiteral("*.json")},
                               QDir::Files, QDir::Name | QDir::IgnoreCase);
            if (jsonFiles.isEmpty()) {
                continue;
            }
            jsonFile = jsonFiles.first();
        }
        candidates.append(VersionFileCandidate{jsonFile});
    }

    QHash<QString, QString> legacyInheritedDisplayNames;
    QSet<QString> loaderBaseVersions;
    for (const VersionFileCandidate &candidate : candidates) {
        QFile versionFile(candidate.jsonFile.absoluteFilePath());
        if (!versionFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument versionDocument =
            QJsonDocument::fromJson(versionFile.readAll());
        if (!versionDocument.isObject()) {
            continue;
        }
        const QJsonObject childRoot = versionDocument.object();
        const QString inheritsFrom =
            childRoot.value(QStringLiteral("inheritsFrom"))
                .toString().trimmed();
        const QString childVersionId =
            childRoot.value(QStringLiteral("id")).toString().trimmed();
        if (inheritsFrom.isEmpty()
            || loaderFromJson(childRoot,
                              childVersionId.isEmpty()
                                  ? candidate.jsonFile.dir().dirName()
                                  : childVersionId,
                              inheritsFrom).name == QStringLiteral("原版")) {
            continue;
        }
        loaderBaseVersions.insert(inheritsFrom);

        QFile childMetadata(QDir(candidate.jsonFile.dir().absolutePath())
                                .filePath(QStringLiteral("launcher.json")));
        if (childMetadata.open(QIODevice::ReadOnly)) {
            const QJsonDocument childDocument =
                QJsonDocument::fromJson(childMetadata.readAll());
            if (childDocument.isObject()
                && !childDocument.object()
                        .value(QStringLiteral("displayName")).toString()
                        .trimmed().isEmpty()) {
                continue;
            }
        }

        QFile baseMetadata(QDir(result.gameDirectory)
                               .filePath(QStringLiteral("versions/%1/launcher.json")
                                             .arg(inheritsFrom)));
        if (!baseMetadata.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument baseDocument =
            QJsonDocument::fromJson(baseMetadata.readAll());
        if (!baseDocument.isObject()) {
            continue;
        }
        const QString displayName =
            baseDocument.object().value(QStringLiteral("displayName"))
                .toString().trimmed();
        if (!displayName.isEmpty()) {
            legacyInheritedDisplayNames.insert(inheritsFrom, displayName);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const VersionFileCandidate &left, const VersionFileCandidate &right) {
                  return left.jsonFile.lastModified() > right.jsonFile.lastModified();
              });

    for (const VersionFileCandidate &candidate : candidates) {
        QFile file(candidate.jsonFile.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }

        const QJsonObject root = document.object();
        QString versionId = root.value(QStringLiteral("id")).toString().trimmed();
        if (versionId.isEmpty()) {
            versionId = candidate.jsonFile.dir().dirName();
        }

        const QString inheritsFrom =
            root.value(QStringLiteral("inheritsFrom")).toString().trimmed();
        const LoaderInfo loader =
            loaderFromJson(root, versionId, inheritsFrom.isEmpty() ? versionId : inheritsFrom);
        if (inheritsFrom.isEmpty() && loaderBaseVersions.contains(versionId)) {
            continue;
        }
        const QString minecraftVersion =
            inheritsFrom.isEmpty() ? versionId : inheritsFrom;
        QFile metadataFile(QDir(candidate.jsonFile.dir().absolutePath())
                               .filePath(QStringLiteral("launcher.json")));
        QString displayName;
        bool loaderBaseOnly = false;
        if (metadataFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument metadata =
                QJsonDocument::fromJson(metadataFile.readAll());
            if (metadata.isObject()) {
                const QJsonObject metadataObject = metadata.object();
                displayName = metadataObject.value(QStringLiteral("displayName"))
                                  .toString().trimmed();
                loaderBaseOnly =
                    metadataObject.value(QStringLiteral("loaderBaseOnly")).toBool();
            }
        }
        if (inheritsFrom.isEmpty() && loaderBaseOnly) {
            continue;
        }
        if (inheritsFrom.isEmpty()
            && legacyInheritedDisplayNames.contains(versionId)) {
            displayName.clear();
        } else if (displayName.isEmpty() && !inheritsFrom.isEmpty()) {
            displayName = legacyInheritedDisplayNames.value(minecraftVersion);
        }
        VersionInfo version;
        version.minecraft = minecraftVersion;
        version.displayName = displayName;
        version.loader = loader.name;
        version.loaderVersion = loader.version;
        version.lastModified = formatLastModified(candidate.jsonFile.lastModified());
        version.playTimeMinutes = 0;
        version.accent = loader.color;
        version.mods = versionMods(candidate.jsonFile.dir(), globalMods,
                                   isolateVersions);
        version.versionId = candidate.jsonFile.dir().dirName();
        version.versionDirectory = candidate.jsonFile.dir().absolutePath();
        version.gameDataDirectory =
            gameDataDirectory(candidate.jsonFile.dir(), result.gameDirectory,
                              isolateVersions);
        result.versions.append(version);
    }

    QSet<QString> seenMods;
    for (const VersionInfo &version : result.versions) {
        for (const ModInfo &mod : version.mods) {
            if (seenMods.contains(mod.name)) {
                continue;
            }
            seenMods.insert(mod.name);
            result.mods.append(mod);
        }
    }
    if (result.versions.isEmpty()) {
        result.mods = globalMods;
        result.error = QStringLiteral("当前目录下没有发现有效的已安装版本。");
    }
    return result;
}

} // namespace LocalVersionScanner
