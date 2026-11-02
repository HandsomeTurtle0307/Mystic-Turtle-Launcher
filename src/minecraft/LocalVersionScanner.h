/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "SampleData.h"

#include <QString>
#include <QVector>

namespace LocalVersionScanner {

struct Result {
    QString gameDirectory;
    QVector<VersionInfo> versions;
    QVector<ModInfo> mods;
    QString error;
};

// Reads <gameDirectory>/versions/*/*.json and resolves mods according to the
// version-isolation setting. Each version records its launch game-data directory.
Result scan(const QString &gameDirectory);

} // namespace LocalVersionScanner
