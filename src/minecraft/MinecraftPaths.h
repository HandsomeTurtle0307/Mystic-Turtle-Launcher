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

namespace MinecraftPaths {

// Returns the conventional Minecraft game directory for the current platform.
// On Windows this is %APPDATA%\.minecraft.
QString defaultGameDirectory();

// Persisted user selection, falling back to defaultGameDirectory().
QString savedGameDirectory();
void saveGameDirectory(const QString &directory);

} // namespace MinecraftPaths
