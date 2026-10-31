/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionParser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QOperatingSystemVersion>
#include <QRegularExpression>
#include <QStringList>
#include <QSysInfo>

namespace {

QString sanitizeDownloadPath(const QString &path)
{
    QString normalized = path.trimmed();
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (normalized.startsWith(QStringLiteral("./"))) {
        normalized.remove(0, 2);
    }
    if (normalized.isEmpty()
        || normalized.startsWith(QLatin1Char('/'))
        || normalized.contains(QLatin1Char(':'))) {
        return QString();
    }

    QStringList parts;
    const QStringList rawParts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : rawParts) {
        if (part == QStringLiteral(".")) {
            continue;
        }
        if (part == QStringLiteral("..")) {
            return QString();
        }
        parts.append(part);
    }
    return parts.join(QLatin1Char('/'));
}

QString mavenDownloadPath(const QString &name, const QString &classifier = QString())
{
    QString coordinate = name.trimmed();
    QString extension = QStringLiteral("jar");
    const qsizetype extensionAt = coordinate.lastIndexOf(QLatin1Char('@'));
    if (extensionAt >= 0) {
        extension = coordinate.mid(extensionAt + 1);
        coordinate.truncate(extensionAt);
    }

    const QStringList parts = coordinate.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }

    QString groupPath = parts.at(0);
    groupPath.replace(QLatin1Char('.'), QLatin1Char('/'));
    const QString artifact = parts.at(1);
    const QString version = parts.at(2);
    QString resolvedClassifier = classifier;
    if (resolvedClassifier.isEmpty() && parts.size() >= 4) {
        resolvedClassifier = parts.at(3);
    }

    QString fileName = QStringLiteral("%1-%2").arg(artifact, version);
    if (!resolvedClassifier.isEmpty()) {
        fileName += QLatin1Char('-') + resolvedClassifier;
    }
    fileName += QLatin1Char('.') + (extension.isEmpty() ? QStringLiteral("jar") : extension);

    const QString path = QStringLiteral("%1/%2/%3/%4")
                             .arg(groupPath, artifact, version, fileName);
    return sanitizeDownloadPath(path);
}

QString currentOsName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("osx");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}

QString currentArchName()
{
    QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch == QStringLiteral("i386") || arch == QStringLiteral("i686")) {
        return QStringLiteral("x86");
    }
    if (arch == QStringLiteral("amd64")) {
        return QStringLiteral("x86_64");
    }
    return arch;
}

QString currentOsVersion()
{
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();
    return QStringLiteral("%1.%2.%3")
        .arg(os.majorVersion())
        .arg(os.minorVersion())
        .arg(os.microVersion());
}

bool ruleMatchesCurrentSystem(const QJsonObject &rule)
{
    const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
    if (!os.isEmpty()) {
        const QString name = os.value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && name.compare(currentOsName(), Qt::CaseInsensitive) != 0) {
            return false;
        }

        const QString arch = os.value(QStringLiteral("arch")).toString();
        if (!arch.isEmpty() && arch.compare(currentArchName(), Qt::CaseInsensitive) != 0) {
            return false;
        }

        const QString versionPattern = os.value(QStringLiteral("version")).toString();
        if (!versionPattern.isEmpty()) {
            const QRegularExpression expression(versionPattern);
            if (!expression.isValid()
                || !expression.match(currentOsVersion()).hasMatch()) {
                return false;
            }
        }
    }

    // The launcher does not enable optional game features such as demo mode.
    const QJsonObject features = rule.value(QStringLiteral("features")).toObject();
    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        if (it.value().toBool()) {
            return false;
        }
    }

    return true;
}

bool rulesAllowCurrentSystem(const QJsonArray &rules)
{
    if (rules.isEmpty()) {
        return true;
    }

    bool allowed = false;
    for (const QJsonValue &value : rules) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject rule = value.toObject();
        if (!ruleMatchesCurrentSystem(rule)) {
            continue;
        }

        const QString action = rule.value(QStringLiteral("action")).toString();
        if (action == QStringLiteral("allow")) {
            allowed = true;
        } else if (action == QStringLiteral("disallow")) {
            allowed = false;
        }
    }
    return allowed;
}

VersionParser::DownloadInfo parseLibraryDownload(const QJsonObject &object,
                                                  const QString &fallbackPath)
{
    VersionParser::DownloadInfo info;
    info.url = object.value(QStringLiteral("url")).toString().trimmed();
    info.sha1 = object.value(QStringLiteral("sha1")).toString().trimmed().toLower();
    info.size = object.contains(QStringLiteral("size"))
        ? static_cast<qint64>(object.value(QStringLiteral("size")).toDouble(-1.0))
        : -1;

    QString path = sanitizeDownloadPath(object.value(QStringLiteral("path")).toString());
    if (path.isEmpty()) {
        path = sanitizeDownloadPath(fallbackPath);
    }
    if (!path.isEmpty()) {
        info.relativePath = QStringLiteral("libraries/%1").arg(path);
    }
    return info;
}

QString expandedNativeClassifier(const QString &classifier)
{
    QString result = classifier.trimmed();
    if (result.contains(QStringLiteral("${arch}"))) {
        const bool is64Bit = QSysInfo::currentCpuArchitecture().contains(
            QStringLiteral("64"), Qt::CaseInsensitive);
        result.replace(QStringLiteral("${arch}"), is64Bit ? QStringLiteral("64")
                                                         : QStringLiteral("32"));
    }
    return result;
}

} // namespace

VersionParser VersionParser::fromFile(const QString &filePath, QString *errorMessage)
{
    VersionParser parser;
    parser.parseFile(filePath, errorMessage);
    return parser;
}

VersionParser VersionParser::fromJson(const QString &jsonData, QString *errorMessage)
{
    VersionParser parser;
    parser.parseJson(jsonData, errorMessage);
    return parser;
}

bool VersionParser::parseFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("无法打开版本 JSON 文件：%1").arg(file.errorString()),
                    errorMessage);
    }

    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return fail(QStringLiteral("读取版本 JSON 文件失败：%1").arg(file.errorString()),
                    errorMessage);
    }

    return parseJson(QString::fromUtf8(data), errorMessage);
}

bool VersionParser::parseJson(const QString &jsonData, QString *errorMessage)
{
    m_valid = false;
    m_error.clear();
    m_version = VersionInfo();
    if (errorMessage) {
        errorMessage->clear();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonData.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("JSON 解析失败（偏移 %1）：%2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()),
                    errorMessage);
    }
    if (!document.isObject()) {
        return fail(QStringLiteral("JSON 根节点必须是对象。"), errorMessage);
    }

    const QJsonObject root = document.object();
    m_version.id = root.value(QStringLiteral("id")).toString().trimmed();
    if (m_version.id.isEmpty()) {
        return fail(QStringLiteral("版本 JSON 缺少 id。"), errorMessage);
    }

    const QJsonObject downloads = root.value(QStringLiteral("downloads")).toObject();
    const QJsonObject clientObject = downloads.value(QStringLiteral("client")).toObject();
    m_version.client.url = clientObject.value(QStringLiteral("url")).toString().trimmed();
    m_version.client.sha1 = clientObject.value(QStringLiteral("sha1")).toString().trimmed().toLower();
    m_version.client.size = clientObject.contains(QStringLiteral("size"))
        ? static_cast<qint64>(clientObject.value(QStringLiteral("size")).toDouble(-1.0))
        : -1;
    m_version.client.relativePath = clientJarRelativePath(m_version.id);

    if (m_version.client.url.isEmpty()) {
        return fail(QStringLiteral("版本 JSON 缺少 client 下载 URL。"), errorMessage);
    }

    const QJsonObject assetIndexObject = root.value(QStringLiteral("assetIndex")).toObject();
    m_version.assetIndexId = assetIndexObject.value(QStringLiteral("id")).toString().trimmed();
    if (m_version.assetIndexId.isEmpty()) {
        m_version.assetIndexId = root.value(QStringLiteral("assets")).toString().trimmed();
    }
    m_version.assetIndex.url =
        assetIndexObject.value(QStringLiteral("url")).toString().trimmed();
    m_version.assetIndex.sha1 =
        assetIndexObject.value(QStringLiteral("sha1")).toString().trimmed().toLower();
    m_version.assetIndex.size = assetIndexObject.contains(QStringLiteral("size"))
        ? static_cast<qint64>(assetIndexObject.value(QStringLiteral("size")).toDouble(-1.0))
        : -1;
    if (!m_version.assetIndexId.isEmpty()) {
        m_version.assetIndex.relativePath =
            assetIndexRelativePath(m_version.assetIndexId);
    }

    // Pre-1.6 version JSONs do not carry a downloadable asset index. The
    // client and libraries can still be installed, so an absent index is not
    // treated as a fatal error.

    const QJsonArray libraries = root.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &value : libraries) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject libraryObject = value.toObject();
        if (!rulesAllowCurrentSystem(libraryObject.value(QStringLiteral("rules")).toArray())) {
            continue;
        }

        const QString name = libraryObject.value(QStringLiteral("name")).toString();
        const QJsonObject libraryDownloads =
            libraryObject.value(QStringLiteral("downloads")).toObject();

        QString baseUrl = libraryObject.value(QStringLiteral("url")).toString().trimmed();
        if (baseUrl.isEmpty()) {
            baseUrl = QStringLiteral("https://libraries.minecraft.net/");
        }
        if (!baseUrl.endsWith(QLatin1Char('/'))) {
            baseUrl += QLatin1Char('/');
        }

        const auto resolveUrl = [&baseUrl](QString *url, const QString &relativePath) {
            if (!url->isEmpty() || relativePath.isEmpty()) {
                return;
            }
            QString path = relativePath;
            if (path.startsWith(QStringLiteral("libraries/"))) {
                path.remove(0, 10);
            }
            *url = baseUrl + path;
        };

        const QString artifactPath = mavenDownloadPath(name);
        const QJsonObject artifactObject =
            libraryDownloads.value(QStringLiteral("artifact")).toObject();
        DownloadInfo artifact;
        if (!artifactObject.isEmpty()) {
            artifact = parseLibraryDownload(artifactObject, artifactPath);
        } else if (libraryDownloads.isEmpty() && !artifactPath.isEmpty()) {
            artifact.relativePath = QStringLiteral("libraries/%1").arg(artifactPath);
        }
        resolveUrl(&artifact.url, artifact.relativePath);
        if (!artifact.url.isEmpty() && !artifact.relativePath.isEmpty()) {
            Library library;
            library.name = name;
            library.type = LibraryType::Java;
            library.download = artifact;
            m_version.libraries.append(library);
        }

        const QJsonObject natives = libraryObject.value(QStringLiteral("natives")).toObject();
        const QString rawClassifier = natives.value(currentOsName()).toString().trimmed();
        if (rawClassifier.isEmpty()) {
            continue;
        }

        const QString classifier = expandedNativeClassifier(rawClassifier);
        const QJsonObject nativeObject = libraryDownloads
                                             .value(QStringLiteral("classifiers"))
                                             .toObject()
                                             .value(classifier)
                                             .toObject();
        const QString nativePath = mavenDownloadPath(name, classifier);
        DownloadInfo nativeDownload;
        if (!nativeObject.isEmpty()) {
            nativeDownload = parseLibraryDownload(nativeObject, nativePath);
        } else if (libraryDownloads.isEmpty() && !nativePath.isEmpty()) {
            nativeDownload.relativePath = QStringLiteral("libraries/%1").arg(nativePath);
        }
        resolveUrl(&nativeDownload.url, nativeDownload.relativePath);
        if (!nativeDownload.url.isEmpty() && !nativeDownload.relativePath.isEmpty()) {
            Library library;
            library.name = name;
            library.classifier = classifier;
            library.type = LibraryType::Native;
            library.download = nativeDownload;
            m_version.libraries.append(library);
        }
    }

    m_valid = true;
    m_error.clear();
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool VersionParser::isValid() const
{
    return m_valid;
}

QString VersionParser::errorString() const
{
    return m_error;
}

const VersionParser::VersionInfo &VersionParser::version() const
{
    return m_version;
}

const QVector<VersionParser::Library> &VersionParser::libraries() const
{
    return m_version.libraries;
}

QString VersionParser::clientJarRelativePath(const QString &versionId)
{
    QString id = versionId.trimmed();
    id.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char(':'))
        || id == QStringLiteral(".") || id == QStringLiteral("..")) {
        return QString();
    }
    return QStringLiteral("versions/%1/%1.jar").arg(id);
}

QString VersionParser::assetIndexRelativePath(const QString &assetIndexId)
{
    QString id = assetIndexId.trimmed();
    id.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char(':'))
        || id == QStringLiteral(".") || id == QStringLiteral("..")) {
        return QString();
    }
    return QStringLiteral("assets/indexes/%1.json").arg(id);
}

QString VersionParser::libraryRelativePath(const QString &downloadPath)
{
    const QString sanitized = sanitizeDownloadPath(downloadPath);
    if (sanitized.isEmpty()) {
        return QString();
    }
    return QStringLiteral("libraries/%1").arg(sanitized);
}

bool VersionParser::fail(const QString &message, QString *errorMessage)
{
    m_valid = false;
    m_error = message;
    m_version = VersionInfo();
    if (errorMessage) {
        *errorMessage = message;
    }
    return false;
}
