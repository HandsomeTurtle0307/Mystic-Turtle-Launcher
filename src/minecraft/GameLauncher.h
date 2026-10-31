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

    // Builds and starts the Minecraft process. Returns false and fills error
    // when required local files or Java are missing.
    static bool launch(const QString &gameDirectory,
                       const QString &versionId,
                       const PlayerAccount &account,
                       const QString &javaExecutable = QString(),
                       const ProgressCallback &progress = {},
                       qint64 *processId = nullptr,
                       QString *errorMessage = nullptr);

    static QString findJavaExecutable(const QString &gameDirectory);
    static QString findCompatibleJava(const QString &gameDirectory,
                                      int requiredMajor,
                                      QString *errorMessage = nullptr);
    static int requiredJavaMajor(const QString &gameDirectory,
                                 const QString &versionId);
    static int javaMajorVersion(const QString &javaExecutable);
};
