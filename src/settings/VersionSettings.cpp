/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

QString VersionSettings::settingsFilePath(const QString &versionDirectory)
{
    return QDir(versionDirectory).filePath(QStringLiteral("settings.json"));
}

VersionSettings VersionSettings::load(const QString &versionDirectory,
                                      QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    VersionSettings settings;
    const QString path = settingsFilePath(versionDirectory);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (QFileInfo::exists(path) && errorMessage) {
            *errorMessage = QStringLiteral("无法读取版本独立设置：%1")
                                .arg(file.errorString());
        }
        return settings;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立设置 JSON 无效：%1")
                                .arg(parseError.errorString());
        }
        return settings;
    }
    if (!document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立设置必须是 JSON 对象。");
        }
        return settings;
    }

    const QJsonObject root = document.object();
    const QJsonValue formatVersion = root.value(QStringLiteral("formatVersion"));
    if (!formatVersion.isUndefined()
        && (!formatVersion.isDouble() || formatVersion.toInt(-1) != 1)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("不支持的版本独立设置格式。");
        }
        return settings;
    }
    const QJsonValue memoryValue = root.value(QStringLiteral("memory"));
    const QJsonValue advancedValue = root.value(QStringLiteral("advanced"));
    if ((!memoryValue.isUndefined() && !memoryValue.isObject())
        || (!advancedValue.isUndefined() && !advancedValue.isObject())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立设置中的 memory 或 advanced 项无效。");
        }
        return settings;
    }
    const QJsonObject memory = root.value(QStringLiteral("memory")).toObject();
    const QJsonObject advanced = root.value(QStringLiteral("advanced")).toObject();
    const QJsonValue memoryOverride = memory.value(QStringLiteral("override"));
    const QJsonValue jvmOverride =
        advanced.value(QStringLiteral("overrideJvmArguments"));
    if ((!memoryOverride.isUndefined() && !memoryOverride.isBool())
        || (!jvmOverride.isUndefined() && !jvmOverride.isBool())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立设置中的覆盖开关必须是布尔值。");
        }
        return settings;
    }
    settings.overrideMemory =
        memoryOverride.toBool(false);
    if (settings.overrideMemory
        && !memory.value(QStringLiteral("customMb")).isDouble()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立内存设置缺少有效的 customMb 数值。");
        }
        return VersionSettings();
    }
    settings.memoryMb = qBound(
        0, memory.value(QStringLiteral("customMb")).toInt(),
        GlobalSettings::physicalMemoryMb());
    settings.overrideJvmArguments =
        jvmOverride.toBool(false);
    if (settings.overrideJvmArguments
        && !advanced.value(QStringLiteral("jvmArgs")).isString()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立 JVM 参数设置缺少有效的 jvmArgs 字符串。");
        }
        return VersionSettings();
    }
    settings.jvmArguments =
        advanced.value(QStringLiteral("jvmArgs")).toString();
    return settings;
}

bool VersionSettings::save(const QString &versionDirectory,
                           QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const QString path = settingsFilePath(versionDirectory);
    if (!overrideMemory && !overrideJvmArguments) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法移除版本独立设置文件：%1").arg(path);
            }
            return false;
        }
        return true;
    }

    if (!QDir().mkpath(versionDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建版本目录：%1").arg(versionDirectory);
        }
        return false;
    }

    QJsonObject memory;
    memory.insert(QStringLiteral("override"), overrideMemory);
    memory.insert(QStringLiteral("customMb"),
                  qBound(0, memoryMb, GlobalSettings::physicalMemoryMb()));

    QJsonObject advanced;
    advanced.insert(QStringLiteral("overrideJvmArguments"), overrideJvmArguments);
    advanced.insert(QStringLiteral("jvmArgs"), jvmArguments);

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("memory"), memory);
    root.insert(QStringLiteral("advanced"), advanced);

    QSaveFile file(path);
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size() || !file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法保存版本独立设置：%1")
                                .arg(file.errorString());
        }
        return false;
    }
    return true;
}

GlobalSettings VersionSettings::applyTo(GlobalSettings settings) const
{
    if (overrideMemory) {
        settings.memory.automatic = false;
        settings.memory.customMb =
            qBound(0, memoryMb, GlobalSettings::physicalMemoryMb());
    }
    if (overrideJvmArguments) {
        settings.advanced.jvmArgs = jvmArguments;
        settings.advanced.jvmPreset = QStringLiteral("custom");
    }
    return settings;
}
