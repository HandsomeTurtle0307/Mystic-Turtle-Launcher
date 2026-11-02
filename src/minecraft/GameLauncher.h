/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QString>

#include <functional>

struct PlayerAccount;

class GameLauncher
{
public:
    using ProgressCallback = std::function<void(int percent, const QString &status)>;
    using CancelCallback = std::function<bool()>;

    // Builds and starts the Minecraft process. gameDirectory contains shared
    // versions/libraries/assets; gameDataDirectory is the selected instance's
    // working directory, or empty to use gameDirectory for both.
    static bool launch(const QString &gameDirectory,
                       const QString &versionId,
                       const PlayerAccount &account,
                       const QString &javaExecutable = QString(),
                       const ProgressCallback &progress = {},
                       qint64 *processId = nullptr,
                       QString *errorMessage = nullptr,
                       const QString &gameDataDirectory = QString(),
                       const CancelCallback &cancel = {});

    static QString findJavaExecutable(const QString &gameDirectory);
    static QString findCompatibleJava(const QString &gameDirectory,
                                      int requiredMajor,
                                      QString *errorMessage = nullptr);
    static int requiredJavaMajor(const QString &gameDirectory,
                                 const QString &versionId);
    static int javaMajorVersion(const QString &javaExecutable);
};
