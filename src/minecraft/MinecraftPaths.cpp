/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "MinecraftPaths.h"

#include <QDir>
#include <QSettings>
#include <QtGlobal>

namespace {

const QString kSettingsOrganization = QStringLiteral("MysticTurtle");
const QString kSettingsApplication = QStringLiteral("MysticTurtle");
const QString kGameDirectoryKey = QStringLiteral("gameDirectory");

} // namespace

QString MinecraftPaths::defaultGameDirectory()
{
#if defined(Q_OS_WIN)
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        return QDir(appData).filePath(QStringLiteral(".minecraft"));
    }
#endif
    return QDir::home().filePath(QStringLiteral(".minecraft"));
}

QString MinecraftPaths::savedGameDirectory()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       kSettingsOrganization, kSettingsApplication);
    const QString stored = settings.value(kGameDirectoryKey).toString().trimmed();
    if (!stored.isEmpty()) {
        return QDir(stored).absolutePath();
    }

    // Migrate the path saved by older builds through the native QSettings store.
    QSettings legacySettings;
    const QString legacy =
        legacySettings.value(kGameDirectoryKey).toString().trimmed();
    if (!legacy.isEmpty()) {
        const QString absolute = QDir(legacy).absolutePath();
        saveGameDirectory(absolute);
        legacySettings.remove(kGameDirectoryKey);
        return absolute;
    }

    return defaultGameDirectory();
}

void MinecraftPaths::saveGameDirectory(const QString &directory)
{
    if (directory.trimmed().isEmpty()) {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       kSettingsOrganization, kSettingsApplication);
    settings.setValue(kGameDirectoryKey, QDir(directory).absolutePath());
    settings.sync();
}
