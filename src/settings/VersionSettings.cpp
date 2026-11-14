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

namespace {

QString normalizeWindowSize(const QString &value)
{
    if (value == QStringLiteral("fullscreen")
        || value == QStringLiteral("borderless")) {
        return value;
    }
    return QStringLiteral("default");
}

QString normalizeJvmPreset(const QString &value)
{
    if (value == QStringLiteral("g1gc") || value == QStringLiteral("zgc")) {
        return value;
    }
    return QStringLiteral("custom");
}

bool readBool(const QJsonObject &object, const QString &key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

} // namespace

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
    const int format = formatVersion.isUndefined() ? 1 : formatVersion.toInt(-1);
    if (format != 1 && format != 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("不支持的版本独立设置格式。");
        }
        return VersionSettings();
    }

    const QJsonValue memoryValue = root.value(QStringLiteral("memory"));
    const QJsonValue advancedValue = root.value(QStringLiteral("advanced"));
    const QJsonValue javaValue = root.value(QStringLiteral("java"));
    const QJsonValue windowValue = root.value(QStringLiteral("window"));
    const QJsonValue launchValue = root.value(QStringLiteral("launch"));
    if ((!memoryValue.isUndefined() && !memoryValue.isObject())
        || (!advancedValue.isUndefined() && !advancedValue.isObject())
        || (!javaValue.isUndefined() && !javaValue.isObject())
        || (!windowValue.isUndefined() && !windowValue.isObject())
        || (!launchValue.isUndefined() && !launchValue.isObject())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立设置的 JSON 分组格式无效。");
        }
        return VersionSettings();
    }

    const QJsonObject memory = memoryValue.toObject();
    const QJsonObject advanced = advancedValue.toObject();
    settings.overrideMemory = readBool(memory, QStringLiteral("override"), false);
    settings.automaticMemory = readBool(memory, QStringLiteral("automatic"), false);
    if (settings.overrideMemory && !settings.automaticMemory
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
        readBool(advanced, QStringLiteral("overrideJvmArguments"), false);
    settings.jvmArguments = advanced.value(QStringLiteral("jvmArgs")).toString();
    settings.jvmPreset = normalizeJvmPreset(
        advanced.value(QStringLiteral("jvmPreset"))
            .toString(settings.overrideJvmArguments ? QStringLiteral("custom")
                                                    : QStringLiteral("g1gc")));
    if (settings.overrideJvmArguments
        && !advanced.value(QStringLiteral("jvmArgs")).isString()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("版本独立 JVM 参数设置缺少有效的 jvmArgs 字符串。");
        }
        return VersionSettings();
    }

    const QJsonObject java = javaValue.toObject();
    settings.overrideJavaPath = readBool(java, QStringLiteral("override"), false);
    settings.javaPath = java.value(QStringLiteral("path")).toString();
    if (settings.overrideJavaPath && settings.javaPath.trimmed().isEmpty()) {
        settings.overrideJavaPath = false;
    }

    const QJsonObject window = windowValue.toObject();
    settings.overrideWindowSize = readBool(window, QStringLiteral("override"), false);
    settings.windowSize = normalizeWindowSize(
        window.value(QStringLiteral("size")).toString(QStringLiteral("default")));

    const QJsonObject launch = launchValue.toObject();
    settings.overrideGameArgs = readBool(launch, QStringLiteral("override"), false);
    settings.gameArgs = launch.value(QStringLiteral("args")).toString();
    if (settings.overrideGameArgs && settings.gameArgs.trimmed().isEmpty()) {
        settings.overrideGameArgs = false;
    }
    return settings;
}

bool VersionSettings::hasOverrides() const
{
    return overrideMemory || overrideJvmArguments || overrideJavaPath
        || overrideWindowSize || overrideGameArgs;
}

bool VersionSettings::save(const QString &versionDirectory,
                           QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    const QString path = settingsFilePath(versionDirectory);
    if (!hasOverrides()) {
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
    memory.insert(QStringLiteral("automatic"), automaticMemory);
    memory.insert(QStringLiteral("customMb"),
                  qBound(0, memoryMb, GlobalSettings::physicalMemoryMb()));

    QJsonObject advanced;
    advanced.insert(QStringLiteral("overrideJvmArguments"), overrideJvmArguments);
    advanced.insert(QStringLiteral("jvmArgs"), jvmArguments);
    advanced.insert(QStringLiteral("jvmPreset"), normalizeJvmPreset(jvmPreset));

    QJsonObject java;
    java.insert(QStringLiteral("override"), overrideJavaPath);
    java.insert(QStringLiteral("path"), javaPath);

    QJsonObject window;
    window.insert(QStringLiteral("override"), overrideWindowSize);
    window.insert(QStringLiteral("size"), normalizeWindowSize(windowSize));

    QJsonObject launch;
    launch.insert(QStringLiteral("override"), overrideGameArgs);
    launch.insert(QStringLiteral("args"), gameArgs);

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), 2);
    root.insert(QStringLiteral("memory"), memory);
    root.insert(QStringLiteral("advanced"), advanced);
    root.insert(QStringLiteral("java"), java);
    root.insert(QStringLiteral("window"), window);
    root.insert(QStringLiteral("launch"), launch);

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
        settings.memory.automatic = automaticMemory;
        if (!automaticMemory) {
            settings.memory.customMb =
                qBound(0, memoryMb, GlobalSettings::physicalMemoryMb());
        }
    }
    if (overrideJvmArguments) {
        settings.advanced.jvmArgs = jvmArguments;
        settings.advanced.jvmPreset = normalizeJvmPreset(jvmPreset);
    }
    if (overrideJavaPath && !javaPath.trimmed().isEmpty()) {
        settings.advanced.javaPath = javaPath.trimmed();
        settings.advanced.autoDownloadJava = false;
    }
    if (overrideWindowSize) {
        settings.launch.windowSize = normalizeWindowSize(windowSize);
    }
    if (overrideGameArgs) {
        const QString extra = gameArgs.trimmed();
        if (!extra.isEmpty()) {
            settings.advanced.gameArgs =
                settings.advanced.gameArgs.trimmed().isEmpty()
                    ? extra
                    : settings.advanced.gameArgs.trimmed() + QLatin1Char(' ') + extra;
        }
    }
    return settings;
}
