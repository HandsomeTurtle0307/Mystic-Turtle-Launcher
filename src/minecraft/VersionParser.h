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
#include <QVector>
#include <QtGlobal>

class QJsonArray;
class QJsonObject;

// Parses a Minecraft version JSON into the files needed by the Windows client.
//
// Save paths are relative to the .minecraft directory and use '/' separators:
//   client jar   versions/<versionId>/<versionId>.jar
//   asset index  assets/indexes/<assetIndexId>.json
//   library jar  libraries/<downloads.artifact.path>
//   native jar   libraries/<downloads.classifiers["natives-windows"].path>
class VersionParser
{
public:
    enum class LibraryType {
        Java,
        Native
    };

    struct DownloadInfo {
        QString url;
        QString sha1;
        QString relativePath;
        qint64 size = -1;
    };

    struct Library {
        QString name;
        QString classifier;
        LibraryType type = LibraryType::Java;
        DownloadInfo download;

        bool isNative() const { return type == LibraryType::Native; }
    };

    struct VersionInfo {
        QString id;
        DownloadInfo client;
        QString assetIndexId;
        DownloadInfo assetIndex;
        QVector<Library> libraries;
    };

    VersionParser() = default;

    // Convenience entry points. The parser is ready immediately; check
    // isValid() / errorString() (or pass errorMessage) afterwards.
    static VersionParser fromFile(const QString &filePath, QString *errorMessage = nullptr);
    static VersionParser fromJson(const QString &jsonData, QString *errorMessage = nullptr);

    // Both entry points return false on malformed input and fill errorMessage
    // when it is non-null. Their state can also be checked with isValid() and
    // errorString().
    bool parseFile(const QString &filePath, QString *errorMessage = nullptr);
    bool parseJson(const QString &jsonData, QString *errorMessage = nullptr);

    bool isValid() const;
    QString errorString() const;

    const VersionInfo &version() const;
    const QVector<Library> &libraries() const;

    static QString clientJarRelativePath(const QString &versionId);
    static QString assetIndexRelativePath(const QString &assetIndexId);
    static QString libraryRelativePath(const QString &downloadPath);

private:
    bool fail(const QString &message, QString *errorMessage);

    VersionInfo m_version;
    QString m_error;
    bool m_valid = false;
};
