/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "MojangMirror.h"

#include <QUrl>

namespace MojangMirror {

QString optimizedUrl(const QString &originalUrl)
{
    const QUrl original(originalUrl);
    if (!original.isValid()) {
        return originalUrl;
    }

    const QString host = original.host().toLower();
    if (host == QStringLiteral("libraries.minecraft.net")) {
        QUrl mirrored = original;
        mirrored.setScheme(QStringLiteral("https"));
        mirrored.setHost(QStringLiteral("bmclapi2.bangbang93.com"));
        mirrored.setPort(-1);
        mirrored.setPath(QStringLiteral("/maven") + original.path());
        return mirrored.toString(QUrl::FullyEncoded);
    }

    if (host != QStringLiteral("launcher.mojang.com")
        && host != QStringLiteral("piston-data.mojang.com")
        && host != QStringLiteral("piston-meta.mojang.com")
        && host != QStringLiteral("launchermeta.mojang.com")) {
        return originalUrl;
    }

    QUrl mirrored = original;
    mirrored.setScheme(QStringLiteral("https"));
    mirrored.setHost(QStringLiteral("bmclapi2.bangbang93.com"));
    mirrored.setPort(-1);
    return mirrored.toString(QUrl::FullyEncoded);
}

} // namespace MojangMirror
