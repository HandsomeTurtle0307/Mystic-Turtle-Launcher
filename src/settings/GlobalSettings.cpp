/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "GlobalSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString readString(const QJsonObject &object, const QString &key,
                   const QString &fallback = QString())
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : fallback;
}

bool readBool(const QJsonObject &object, const QString &key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

int readInt(const QJsonObject &object, const QString &key, int fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt() : fallback;
}

} // namespace

QString GlobalSettings::settingsFilePath()
{
    QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QStringLiteral("/.mysticturtle");
    }
    return QDir(directory).filePath(QStringLiteral("global-settings.json"));
}

QString GlobalSettings::defaultJvmArguments()
{
    return QStringLiteral(
        "-XX:+UseG1GC -XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 "
        "-XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC "
        "-XX:+AlwaysPreTouch -XX:G1NewSizePercent=30 "
        "-XX:G1MaxNewSizePercent=40 -XX:G1HeapRegionSize=8M "
        "-XX:G1ReservePercent=20 -XX:G1HeapWastePercent=5 "
        "-XX:G1MixedGCCountTarget=4 -XX:InitiatingHeapOccupancyPercent=15 "
        "-XX:G1MixedGCLiveThresholdPercent=90 "
        "-XX:G1RSetUpdatingPauseTimePercent=5 -XX:SurvivorRatio=32 "
        "-XX:+PerfDisableSharedMem -XX:MaxTenuringThreshold=1");
}

GlobalSettings GlobalSettings::load()
{
    GlobalSettings settings;
    settings.advanced.jvmArgs = defaultJvmArguments();

    const QString path = settingsFilePath();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            return fromJson(document.object());
        }
    }

    // One-time migration from the previous QSettings storage.
    const QSettings legacy;
    const QString legacyJava = legacy.value(QStringLiteral("game/javaPath")).toString();
    if (!legacyJava.isEmpty()) {
        settings.advanced.javaPath = legacyJava;
    }
    const QString legacySource =
        legacy.value(QStringLiteral("network/downloadSource"),
                     QStringLiteral("bmclapi")).toString();
    settings.network.downloadSource = legacySource == QStringLiteral("official")
        ? QStringLiteral("official")
        : QStringLiteral("bmclapi");
    if (legacy.contains(QStringLiteral("game/memoryGb"))) {
        const int legacyGb =
            qBound(1, legacy.value(QStringLiteral("game/memoryGb")).toInt(), 32);
        settings.memory.automatic = false;
        settings.memory.customMb = legacyGb * 1024;
    }
    return settings;
}

bool GlobalSettings::save() const
{
    const QString path = settingsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        return false;
    }

    // Keep the legacy keys in sync for code paths that still read them.
    QSettings legacy;
    legacy.setValue(QStringLiteral("game/javaPath"), advanced.javaPath);
    legacy.setValue(QStringLiteral("game/memoryGb"),
                    qMax(0, effectiveMemoryMb() / 1024));
    legacy.setValue(QStringLiteral("network/downloadSource"), network.downloadSource);
    legacy.sync();
    return true;
}

QJsonObject GlobalSettings::toJson() const
{
    QJsonObject launchObject;
    launchObject.insert(QStringLiteral("windowTitle"), launch.windowTitle);
    launchObject.insert(QStringLiteral("customInfo"), launch.customInfo);
    launchObject.insert(QStringLiteral("launcherVisibility"), launch.launcherVisibility);
    launchObject.insert(QStringLiteral("processPriority"), launch.processPriority);
    launchObject.insert(QStringLiteral("windowSize"), launch.windowSize);
    launchObject.insert(QStringLiteral("crashBehavior"), launch.crashBehavior);

    QJsonObject networkObject;
    networkObject.insert(QStringLiteral("downloadSource"), network.downloadSource);
    networkObject.insert(QStringLiteral("proxyType"), network.proxyType);
    networkObject.insert(QStringLiteral("proxyHost"), network.proxyHost);
    networkObject.insert(QStringLiteral("proxyPort"), network.proxyPort);
    networkObject.insert(QStringLiteral("concurrent"), network.concurrent);

    QJsonObject homepageObject;
    homepageObject.insert(QStringLiteral("mode"), homepage.mode);
    homepageObject.insert(QStringLiteral("blankAnimation"), homepage.blankAnimation);
    homepageObject.insert(QStringLiteral("blankAnimationStyle"),
                          homepage.blankAnimationStyle);
    homepageObject.insert(QStringLiteral("live2dBreathAmplitude"),
                          homepage.live2dBreathAmplitude);
    homepageObject.insert(QStringLiteral("live2dSwayAmplitude"),
                          homepage.live2dSwayAmplitude);
    homepageObject.insert(QStringLiteral("live2dEyeAmplitude"),
                          homepage.live2dEyeAmplitude);

    QJsonObject memoryObject;
    memoryObject.insert(QStringLiteral("automatic"), memory.automatic);
    memoryObject.insert(QStringLiteral("customMb"), memory.customMb);
    memoryObject.insert(QStringLiteral("optimizeBeforeLaunch"),
                        memory.optimizeBeforeLaunch);

    QJsonObject skinObject;
    skinObject.insert(QStringLiteral("mode"), skin.mode);
    skinObject.insert(QStringLiteral("premiumName"), skin.premiumName);
    skinObject.insert(QStringLiteral("customPath"), skin.customPath);

    QJsonObject advancedObject;
    advancedObject.insert(QStringLiteral("jvmArgs"), advanced.jvmArgs);
    advancedObject.insert(QStringLiteral("jvmPreset"), advanced.jvmPreset);
    advancedObject.insert(QStringLiteral("gameArgs"), advanced.gameArgs);
    advancedObject.insert(QStringLiteral("preLaunchCommand"), advanced.preLaunchCommand);
    advancedObject.insert(QStringLiteral("javaPath"), advanced.javaPath);
    advancedObject.insert(QStringLiteral("autoDownloadJava"), advanced.autoDownloadJava);
    advancedObject.insert(QStringLiteral("memoryManagement"),
                          advanced.memoryManagement);
    advancedObject.insert(QStringLiteral("disableLaunchWrapper"),
                          advanced.disableLaunchWrapper);
    advancedObject.insert(QStringLiteral("disableLwjglUnsafeAgent"),
                          advanced.disableLwjglUnsafeAgent);
    advancedObject.insert(QStringLiteral("preferHighPerformanceGpu"),
                          advanced.preferHighPerformanceGpu);

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), formatVersion);
    root.insert(QStringLiteral("versionIsolation"), versionIsolation);
    root.insert(QStringLiteral("launch"), launchObject);
    root.insert(QStringLiteral("network"), networkObject);
    root.insert(QStringLiteral("homepage"), homepageObject);
    root.insert(QStringLiteral("memory"), memoryObject);
    root.insert(QStringLiteral("skin"), skinObject);
    root.insert(QStringLiteral("advanced"), advancedObject);
    return root;
}

GlobalSettings GlobalSettings::fromJson(const QJsonObject &object)
{
    GlobalSettings settings;
    settings.advanced.jvmArgs = defaultJvmArguments();
    settings.formatVersion = object.value(QStringLiteral("formatVersion")).toInt(1);
    if (settings.formatVersion <= 0) {
        settings.formatVersion = 1;
    }
    const QString isolation =
        readString(object, QStringLiteral("versionIsolation"),
                   settings.versionIsolation);
    if (isolation == QStringLiteral("isolate_all")
        || isolation == QStringLiteral("isolate_loaders")
        || isolation == QStringLiteral("none")) {
        settings.versionIsolation = isolation;
    }

    const QJsonObject launchObject = object.value(QStringLiteral("launch")).toObject();
    settings.launch.windowTitle =
        readString(launchObject, QStringLiteral("windowTitle"));
    settings.launch.customInfo =
        readString(launchObject, QStringLiteral("customInfo"));
    settings.launch.launcherVisibility =
        readString(launchObject, QStringLiteral("launcherVisibility"),
                   settings.launch.launcherVisibility);
    settings.launch.processPriority =
        readString(launchObject, QStringLiteral("processPriority"),
                   settings.launch.processPriority);
    settings.launch.windowSize =
        readString(launchObject, QStringLiteral("windowSize"),
                   settings.launch.windowSize);
    settings.launch.crashBehavior =
        readString(launchObject, QStringLiteral("crashBehavior"),
                   settings.launch.crashBehavior);

    const QJsonObject networkObject =
        object.value(QStringLiteral("network")).toObject();
    settings.network.downloadSource =
        readString(networkObject, QStringLiteral("downloadSource"),
                   settings.network.downloadSource);
    settings.network.proxyType =
        readString(networkObject, QStringLiteral("proxyType"),
                   settings.network.proxyType);
    settings.network.proxyHost =
        readString(networkObject, QStringLiteral("proxyHost"));
    settings.network.proxyPort =
        qBound(1, readInt(networkObject, QStringLiteral("proxyPort"),
                          settings.network.proxyPort), 65535);
    settings.network.concurrent =
        qBound(1, readInt(networkObject, QStringLiteral("concurrent"),
                          settings.network.concurrent), 8);

    const QJsonObject homepageObject =
        object.value(QStringLiteral("homepage")).toObject();
    QString homepageMode =
        readString(homepageObject, QStringLiteral("mode"),
                   settings.homepage.mode);
    if (homepageMode == QStringLiteral("preset")
        || homepageMode == QStringLiteral("custom")) {
        homepageMode = QStringLiteral("news");
    }
    if (homepageMode == QStringLiteral("blank")
        || homepageMode == QStringLiteral("news")
        || homepageMode == QStringLiteral("status")) {
        settings.homepage.mode = homepageMode;
    }
    settings.homepage.blankAnimation =
        readBool(homepageObject, QStringLiteral("blankAnimation"), true);
    QString blankAnimationStyle =
        readString(homepageObject, QStringLiteral("blankAnimationStyle"),
                   settings.homepage.blankAnimationStyle);
    if (blankAnimationStyle == QStringLiteral("live2d")) {
        blankAnimationStyle = QStringLiteral("live2d-maid");
    }
    if (blankAnimationStyle != QStringLiteral("turtle")
        && !blankAnimationStyle.startsWith(QStringLiteral("live2d-"))) {
        blankAnimationStyle = QStringLiteral("turtle");
    }
    settings.homepage.blankAnimationStyle = blankAnimationStyle;
    settings.homepage.live2dBreathAmplitude = qBound(
        0, readInt(homepageObject, QStringLiteral("live2dBreathAmplitude"),
                   settings.homepage.live2dBreathAmplitude), 100);
    settings.homepage.live2dSwayAmplitude = qBound(
        0, readInt(homepageObject, QStringLiteral("live2dSwayAmplitude"),
                   settings.homepage.live2dSwayAmplitude), 100);
    settings.homepage.live2dEyeAmplitude = qBound(
        0, readInt(homepageObject, QStringLiteral("live2dEyeAmplitude"),
                   settings.homepage.live2dEyeAmplitude), 100);

    const QJsonObject memoryObject = object.value(QStringLiteral("memory")).toObject();
    settings.memory.automatic =
        readBool(memoryObject, QStringLiteral("automatic"), settings.memory.automatic);
    settings.memory.customMb =
        qBound(0, readInt(memoryObject, QStringLiteral("customMb"),
                          settings.memory.customMb), physicalMemoryMb());
    settings.memory.optimizeBeforeLaunch =
        readBool(memoryObject, QStringLiteral("optimizeBeforeLaunch"),
                 settings.memory.optimizeBeforeLaunch);

    const QJsonObject skinObject = object.value(QStringLiteral("skin")).toObject();
    settings.skin.mode = readString(skinObject, QStringLiteral("mode"),
                                    settings.skin.mode);
    settings.skin.premiumName =
        readString(skinObject, QStringLiteral("premiumName"));
    settings.skin.customPath =
        readString(skinObject, QStringLiteral("customPath"));

    const QJsonObject advancedObject =
        object.value(QStringLiteral("advanced")).toObject();
    settings.advanced.jvmArgs =
        readString(advancedObject, QStringLiteral("jvmArgs"),
                   defaultJvmArguments());
    settings.advanced.jvmPreset =
        readString(advancedObject, QStringLiteral("jvmPreset"),
                   settings.advanced.jvmPreset);
    settings.advanced.gameArgs =
        readString(advancedObject, QStringLiteral("gameArgs"));
    settings.advanced.preLaunchCommand =
        readString(advancedObject, QStringLiteral("preLaunchCommand"));
    settings.advanced.javaPath =
        readString(advancedObject, QStringLiteral("javaPath"));
    settings.advanced.autoDownloadJava =
        readBool(advancedObject, QStringLiteral("autoDownloadJava"), true);
    settings.advanced.memoryManagement =
        readString(advancedObject, QStringLiteral("memoryManagement"),
                   settings.advanced.memoryManagement);
    settings.advanced.disableLaunchWrapper =
        readBool(advancedObject, QStringLiteral("disableLaunchWrapper"), false);
    settings.advanced.disableLwjglUnsafeAgent =
        readBool(advancedObject, QStringLiteral("disableLwjglUnsafeAgent"), false);
    settings.advanced.preferHighPerformanceGpu =
        readBool(advancedObject, QStringLiteral("preferHighPerformanceGpu"), false);
    return settings;
}

int GlobalSettings::physicalMemoryMb()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<int>(status.ullTotalPhys / (1024ULL * 1024ULL));
    }
#endif
    return 8192;
}

int GlobalSettings::effectiveMemoryMb() const
{
    if (!memory.automatic) {
        return qBound(0, memory.customMb, physicalMemoryMb());
    }
    const int physical = physicalMemoryMb();
    return qBound(512, physical / 2, 8192);
}

bool GlobalSettings::isolatesVersion(const QString &loader) const
{
    if (versionIsolation == QStringLiteral("isolate_all")) {
        return true;
    }
    if (versionIsolation == QStringLiteral("isolate_loaders")) {
        return !loader.trimmed().isEmpty()
            && loader.compare(QStringLiteral("原版"), Qt::CaseInsensitive) != 0;
    }
    return false;
}

void GlobalSettings::applyNetworkProxy() const
{
    QNetworkProxy proxy;
    const QString type = network.proxyType.toLower();
    if ((type != QStringLiteral("http") && type != QStringLiteral("socks5"))
        || network.proxyHost.trimmed().isEmpty()
        || network.proxyPort <= 0) {
        proxy.setType(QNetworkProxy::NoProxy);
    } else {
        proxy.setType(type == QStringLiteral("socks5")
                          ? QNetworkProxy::Socks5Proxy
                          : QNetworkProxy::HttpProxy);
        proxy.setHostName(network.proxyHost.trimmed());
        proxy.setPort(static_cast<quint16>(qBound(1, network.proxyPort, 65535)));
    }
    QNetworkProxy::setApplicationProxy(proxy);
}
