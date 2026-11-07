/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "GlobalSettings.h"

#include <QString>

struct VersionSettings
{
    bool overrideMemory = false;
    int memoryMb = 0;
    bool overrideJvmArguments = false;
    QString jvmArguments;

    static QString settingsFilePath(const QString &versionDirectory);
    static VersionSettings load(const QString &versionDirectory,
                                QString *errorMessage = nullptr);

    bool save(const QString &versionDirectory, QString *errorMessage = nullptr) const;
    GlobalSettings applyTo(GlobalSettings settings) const;
};
