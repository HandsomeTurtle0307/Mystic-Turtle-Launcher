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

namespace MojangMirror {

enum class DownloadSource {
    Bmclapi,
    Official
};

void setDownloadSource(DownloadSource source);
DownloadSource downloadSource();

// Rewrites Mojang CDN hosts to BMCLAPI when the mirror source is selected.
// Non-Mojang URLs are returned unchanged.
QString optimizedUrl(const QString &originalUrl);

} // namespace MojangMirror
