/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

struct MinecraftVersionEntry
{
    QString id;
    QString type;
    QUrl jsonUrl;
    QString jsonSha1;
    QDateTime releaseTime;
};

struct MinecraftVersionManifest
{
    QString latestRelease;
    QString latestSnapshot;
    QVector<MinecraftVersionEntry> versions;
};

Q_DECLARE_METATYPE(MinecraftVersionManifest);

// Maps the manifest's raw type ("release", "snapshot", "old_beta",
// "old_alpha") to the Chinese category used by the UI.
QString versionCategory(const QString &type);

// Fetches the complete Mojang version manifest, falling back to the
// BMCLAPI mirror if the official endpoint is unavailable.
class VersionManifestClient : public QObject
{
    Q_OBJECT

public:
    explicit VersionManifestClient(QObject *parent = nullptr);

    void fetch();
    bool isFetching() const;

signals:
    void ready(const MinecraftVersionManifest &manifest);
    void failed(const QString &reason);

private:
    void requestManifest(bool useMirror);
    void handleReply();

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    bool m_fetching = false;
    bool m_usingMirror = false;
    bool m_fallbackTried = false;
};
