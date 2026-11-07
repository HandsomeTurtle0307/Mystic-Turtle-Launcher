/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QColor>
#include <QString>
#include <QVector>

struct ModInfo {
    QString name;
    QString version;
    QString description;
    QString category;
    bool enabled = true;
    QString filePath;
};

struct VersionInfo {
    QString minecraft;
    QString loader;
    QString loaderVersion;
    QString lastModified;
    int playTimeMinutes = 0;
    QColor accent;
    QVector<ModInfo> mods;
    QString versionId;
    QString versionDirectory;
    QString gameDataDirectory;
    QString displayName;
    bool isolated = false;
};

struct GameRelease {
    QString version;
    QString type;
    QString releaseDate;
    QString notes;
    bool latest = false;
    QColor accent;
};

struct CatalogMod {
    QString name;
    QString author;
    QString summary;
    QString category;
    QString downloads;
    QColor accent;
};

namespace SampleData {

const QVector<VersionInfo> &localVersions();
const QVector<GameRelease> &gameReleases();
const QVector<CatalogMod> &catalogMods();

int enabledModCount(const VersionInfo &version);

} // namespace SampleData
