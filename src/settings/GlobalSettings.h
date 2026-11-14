/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QJsonObject>
#include <QString>

struct GlobalSettings
{
    struct LaunchOptions
    {
        QString windowTitle;
        QString customInfo;
        // keep / minimize / hide
        QString launcherVisibility = QStringLiteral("keep");
        // high / above_normal / normal / below_normal / low
        QString processPriority = QStringLiteral("normal");
        // default / fullscreen / widescreen / windowed / borderless
        QString windowSize = QStringLiteral("default");
        // dialog / silent / keep_open
        QString crashBehavior = QStringLiteral("dialog");
    };

    struct NetworkOptions
    {
        // bmclapi / official
        QString downloadSource = QStringLiteral("bmclapi");
        // none / http / socks5
        QString proxyType = QStringLiteral("none");
        QString proxyHost;
        int proxyPort = 7890;
        int concurrent = 8;
    };

    struct HomepageOptions
    {
        // blank / news / status
        QString mode = QStringLiteral("news");
        bool blankAnimation = true;
        // turtle / live2d-<portrait>
        QString blankAnimationStyle = QStringLiteral("turtle");
        int live2dBreathAmplitude = 55;
        int live2dSwayAmplitude = 55;
        int live2dEyeAmplitude = 60;
    };

    struct MemoryOptions
    {
        bool automatic = true;
        int customMb = 8192;
        bool optimizeBeforeLaunch = true;
    };

    struct SkinOptions
    {
        // random / steve / alex / premium / custom
        QString mode = QStringLiteral("steve");
        QString premiumName;
        QString customPath;
    };

    struct AdvancedOptions
    {
        QString jvmArgs;
        QString jvmPreset = QStringLiteral("custom");
        QString gameArgs;
        QString preLaunchCommand;
        QString javaPath;
        bool autoDownloadJava = true;
        // g1gc / default / zgc / shenandoah
        QString memoryManagement = QStringLiteral("g1gc");
        bool disableLaunchWrapper = false;
        bool disableLwjglUnsafeAgent = false;
        bool preferHighPerformanceGpu = false;
    };

    int formatVersion = 1;
    QString versionIsolation = QStringLiteral("isolate_all");
    LaunchOptions launch;
    NetworkOptions network;
    HomepageOptions homepage;
    MemoryOptions memory;
    SkinOptions skin;
    AdvancedOptions advanced;

    static QString settingsFilePath();
    static GlobalSettings load();
    bool save() const;
    void applyNetworkProxy() const;

    QJsonObject toJson() const;
    static GlobalSettings fromJson(const QJsonObject &object);
    static QString defaultJvmArguments();
    bool isolatesVersion(const QString &loader) const;

    int effectiveMemoryMb() const;
    static int physicalMemoryMb();

private:
};
