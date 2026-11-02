/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DownloadModsPage.h"

#include "AccentButton.h"
#include "AnimatedComboBox.h"
#include "CardWidget.h"
#include "LoadingIndicator.h"
#include "MinecraftPaths.h"
#include "SearchField.h"
#include "Theme.h"
#include "minecraft/LocalVersionScanner.h"
#include "minecraft/VersionManifest.h"
#include "network/DownloadTask.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QVersionNumber>
#include <QSettings>
#include <QScrollArea>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QPointer>
#include <QShowEvent>

#include <algorithm>
#include <functional>

namespace {

const QUrl kModrinthApi(QStringLiteral("https://api.modrinth.com/v2"));
const QString kUserAgent = QStringLiteral("MysticTurtle/0.2 (mod downloader)");
constexpr int kProjectsPerPage = 20;

class ModDetailsDialog : public QDialog
{
public:
    explicit ModDetailsDialog(QWidget *parent)
        : QDialog(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);

        const QSettings settings;
        m_opacity = qBound(
            0, settings.value(QStringLiteral("ui/backgroundOpacity"), 35).toInt(), 100);
        const QString path =
            settings.value(QStringLiteral("ui/backgroundImage")).toString();
        if (!path.isEmpty()) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            m_background = reader.read();
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.fillRect(rect(), Theme::BackgroundDeep);

        if (m_background.isNull() || m_opacity <= 0 || width() <= 0 || height() <= 0) {
            return;
        }

        const qreal targetAspect = static_cast<qreal>(width()) / height();
        const qreal imageAspect = static_cast<qreal>(m_background.width())
            / m_background.height();
        QRectF sourceRect(m_background.rect());
        if (imageAspect > targetAspect) {
            const qreal croppedWidth = m_background.height() * targetAspect;
            sourceRect.setLeft((m_background.width() - croppedWidth) / 2.0);
            sourceRect.setWidth(croppedWidth);
        } else {
            const qreal croppedHeight = m_background.width() / targetAspect;
            sourceRect.setTop((m_background.height() - croppedHeight) / 2.0);
            sourceRect.setHeight(croppedHeight);
        }

        painter.setOpacity(m_opacity / 100.0);
        painter.drawImage(rect(), m_background, sourceRect);
    }

private:
    QImage m_background;
    int m_opacity = 35;
};

QNetworkRequest apiRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setTransferTimeout(30000);
    return request;
}

QString formattedDownloads(qint64 downloads)
{
    return QLocale().toString(downloads);
}

QString formattedDate(const QString &value)
{
    const QDateTime date = QDateTime::fromString(value, Qt::ISODateWithMs).isValid()
        ? QDateTime::fromString(value, Qt::ISODateWithMs)
        : QDateTime::fromString(value, Qt::ISODate);
    return date.isValid() ? date.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"))
                          : QStringLiteral("未知");
}

QString prettyLoader(const QString &loader)
{
    if (loader == QStringLiteral("fabric")) {
        return QStringLiteral("Fabric");
    }
    if (loader == QStringLiteral("neoforge")) {
        return QStringLiteral("NeoForge");
    }
    if (loader == QStringLiteral("forge")) {
        return QStringLiteral("Forge");
    }
    if (loader == QStringLiteral("liteloader")) {
        return QStringLiteral("LiteLoader");
    }
    return loader;
}

void setRemoteIcon(QNetworkAccessManager *network,
                   const QString &url,
                   QLabel *label,
                   const QSize &size)
{
    if (!network || url.isEmpty() || !label) {
        return;
    }

    auto *indicator = new LoadingIndicator(label, Theme::Accent);
    indicator->setFixedSize(20, 20);
    indicator->move((size.width() - indicator->width()) / 2,
                    (size.height() - indicator->height()) / 2);
    QNetworkReply *reply = network->get(apiRequest(QUrl(url)));
    if (!reply) {
        indicator->hide();
        return;
    }
    const QPointer<QLabel> safeLabel(label);
    const QPointer<LoadingIndicator> safeIndicator(indicator);
    QObject::connect(reply, &QNetworkReply::finished, network,
                     [reply, safeLabel, safeIndicator, size]() {
                reply->deleteLater();
                if (safeIndicator) {
                    safeIndicator->hide();
                }
                if (!safeLabel || reply->error() != QNetworkReply::NoError) {
                    return;
                }
                QImage image;
                if (!image.loadFromData(reply->readAll())) {
                    return;
                }
                safeLabel->setPixmap(QPixmap::fromImage(image).scaled(
                    size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
}

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    return Theme::label(text, 10, QFont::Normal, Theme::TextMuted, parent);
}

QWidget *metadataItem(const QString &labelText,
                      const QString &value,
                      QWidget *parent)
{
    auto *item = new QWidget(parent);
    auto *layout = new QVBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(mutedLabel(labelText, item));
    layout->addWidget(Theme::label(value, 11, QFont::DemiBold,
                                   Theme::TextPrimary, item));
    return item;
}

void requestProjectVersions(const QString &projectId,
                            QWidget *dialog,
                            QVBoxLayout *recommendationLayout,
                            QVBoxLayout *versionsLayout,
                            const QString &gameDirectory,
                            const QStringList &gameVersionOrder,
                            const QString &preferredGameVersion,
                            const QString &preferredLoader,
                            const std::function<bool(const QUrl &,
                                                     const QString &,
                                                     const QByteArray &,
                                                     const QString &)> &download)
{
    auto *network = new QNetworkAccessManager(dialog);
    QUrl url(kModrinthApi.toString() + QStringLiteral("/project/")
             + QString::fromUtf8(QUrl::toPercentEncoding(projectId))
             + QStringLiteral("/version"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("include_changelog"), QStringLiteral("false"));
    url.setQuery(query);
    QNetworkReply *reply = network->get(apiRequest(url));
    auto *loadingRow = new QWidget(dialog);
    auto *loadingLayout = new QHBoxLayout(loadingRow);
    loadingLayout->setContentsMargins(0, 0, 0, 0);
    loadingLayout->setSpacing(8);
    auto *indicator = new LoadingIndicator(loadingRow, Theme::Accent);
    indicator->setFixedSize(18, 18);
    loadingLayout->addWidget(indicator);
    auto *status = mutedLabel(QStringLiteral("正在加载版本列表…"), loadingRow);
    loadingLayout->addWidget(status);
    loadingLayout->addStretch();
    versionsLayout->addWidget(loadingRow);

    const auto clearRecommendation = [recommendationLayout]() {
        while (recommendationLayout->count() > 0) {
            QLayoutItem *item = recommendationLayout->takeAt(0);
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
    };
    const auto fail = [dialog, loadingRow, indicator, status,
                       recommendationLayout, preferredGameVersion,
                       preferredLoader, clearRecommendation](const QString &message) {
        indicator->hide();
        status->setText(message);
        if (!preferredGameVersion.isEmpty() || !preferredLoader.isEmpty()) {
            clearRecommendation();
            recommendationLayout->addWidget(mutedLabel(
                QStringLiteral("推荐版本加载失败，请在下方完整列表中重试查看。"),
                dialog));
        }
        QMessageBox::warning(dialog, QStringLiteral("模组详情加载失败"), message);
    };
    if (!reply) {
        fail(QStringLiteral("无法发起模组版本列表请求。"));
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, dialog,
                     [reply, dialog, loadingRow, recommendationLayout, versionsLayout,
                      gameDirectory,
                      gameVersionOrder, preferredGameVersion, preferredLoader,
                      download, clearRecommendation, fail]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("加载模组版本失败：%1").arg(reply->errorString()));
            return;
        }

        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode < 200 || statusCode >= 300) {
            fail(QStringLiteral("加载模组版本失败：HTTP %1").arg(statusCode));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            fail(QStringLiteral("Modrinth 返回了无效的模组版本数据。"));
            return;
        }

        loadingRow->deleteLater();
        clearRecommendation();
        QMap<QString, QVector<QJsonObject>> groups;
        for (const QJsonValue &value : document.array()) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject version = value.toObject();
            const QJsonArray gameVersions =
                version.value(QStringLiteral("game_versions")).toArray();
            if (gameVersions.isEmpty()) {
                groups[QStringLiteral("其他版本")].append(version);
                continue;
            }
            for (const QJsonValue &gameVersion : gameVersions) {
                const QString gameVersionId = gameVersion.toString().trimmed();
                if (!gameVersionId.isEmpty()) {
                    groups[gameVersionId].append(version);
                }
            }
        }

        if (groups.isEmpty()) {
            clearRecommendation();
            if (!preferredGameVersion.isEmpty() || !preferredLoader.isEmpty()) {
                recommendationLayout->addWidget(mutedLabel(
                    QStringLiteral("该模组暂无可用版本。"), dialog));
            }
            versionsLayout->addWidget(mutedLabel(QStringLiteral("该模组暂无可用版本。"), dialog));
            return;
        }

        QStringList gameVersionKeys = groups.keys();
        const auto hasLoader = [&groups](const QString &gameVersionId,
                                         const QString &loader) {
            if (loader.isEmpty()) {
                return false;
            }
            for (const QJsonObject &version : groups.value(gameVersionId)) {
                for (const QJsonValue &value :
                     version.value(QStringLiteral("loaders")).toArray()) {
                    if (value.toString() == loader) {
                        return true;
                    }
                }
            }
            return false;
        };
        std::sort(gameVersionKeys.begin(), gameVersionKeys.end(),
                  [&gameVersionOrder](const QString &left, const QString &right) {
            if (left == QStringLiteral("其他版本")) {
                return false;
            }
            if (right == QStringLiteral("其他版本")) {
                return true;
            }
            const QVersionNumber leftVersion = QVersionNumber::fromString(left);
            const QVersionNumber rightVersion = QVersionNumber::fromString(right);
            if (!leftVersion.isNull() && !rightVersion.isNull()
                && leftVersion != rightVersion) {
                return QVersionNumber::compare(leftVersion, rightVersion) > 0;
            }
            const bool leftPrerelease =
                left.contains(QStringLiteral("-pre"), Qt::CaseInsensitive)
                || left.contains(QStringLiteral("-rc"), Qt::CaseInsensitive);
            const bool rightPrerelease =
                right.contains(QStringLiteral("-pre"), Qt::CaseInsensitive)
                || right.contains(QStringLiteral("-rc"), Qt::CaseInsensitive);
            if (leftPrerelease != rightPrerelease) {
                return !leftPrerelease;
            }
            const int leftIndex = gameVersionOrder.indexOf(left);
            const int rightIndex = gameVersionOrder.indexOf(right);
            if (leftIndex >= 0 && rightIndex >= 0 && leftIndex != rightIndex) {
                return leftIndex < rightIndex;
            }
            return QString::localeAwareCompare(left, right) > 0;
        });

        const auto renderVersionGroup =
            [dialog, gameDirectory, download](
                QVBoxLayout *targetLayout,
                const QString &gameVersionId,
                const QString &groupTitle,
                QVector<QJsonObject> versions,
                const QString &loaderFilter,
                bool expanded) {
            std::sort(versions.begin(), versions.end(),
                      [&loaderFilter](const QJsonObject &left,
                                      const QJsonObject &right) {
                if (!loaderFilter.isEmpty()) {
                    const auto matchesLoader = [&loaderFilter](const QJsonObject &version) {
                        for (const QJsonValue &loader :
                             version.value(QStringLiteral("loaders")).toArray()) {
                            if (loader.toString() == loaderFilter) {
                                return true;
                            }
                        }
                        return false;
                    };
                    const bool leftMatches = matchesLoader(left);
                    const bool rightMatches = matchesLoader(right);
                    if (leftMatches != rightMatches) {
                        return leftMatches;
                    }
                }
                return left.value(QStringLiteral("date_published")).toString()
                    > right.value(QStringLiteral("date_published")).toString();
            });

            auto *group = new QWidget(dialog);
            auto *groupLayout = new QVBoxLayout(group);
            groupLayout->setContentsMargins(0, 0, 0, 8);
            groupLayout->setSpacing(4);

            auto *toggle = new QToolButton(group);
            toggle->setText(QStringLiteral("%1  ·  %2 个版本")
                                .arg(groupTitle)
                                .arg(versions.size()));
            toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            toggle->setArrowType(expanded ? Qt::UpArrow : Qt::DownArrow);
            toggle->setCheckable(true);
            toggle->setChecked(expanded);
            toggle->setStyleSheet(QStringLiteral(
                "QToolButton { color: %1; background: transparent; border: 1px solid %2;"
                "border-radius: 9px; padding: 10px 12px; text-align: left; }"
                "QToolButton:hover { background: rgba(110, 231, 183, 14); }")
                .arg(Theme::TextPrimary.name(), Theme::Border.name()));
            groupLayout->addWidget(toggle);

            auto *items = new QWidget(group);
            auto *itemsLayout = new QVBoxLayout(items);
            itemsLayout->setContentsMargins(8, 3, 0, 4);
            itemsLayout->setSpacing(4);

            for (const QJsonObject &version : versions) {
                const QJsonArray files = version.value(QStringLiteral("files")).toArray();
                QJsonObject file;
                for (const QJsonValue &fileValue : files) {
                    const QJsonObject candidate = fileValue.toObject();
                    if (candidate.value(QStringLiteral("primary")).toBool()) {
                        file = candidate;
                        break;
                    }
                }
                if (file.isEmpty() && !files.isEmpty()) {
                    file = files.first().toObject();
                }
                const QString fileUrl = file.value(QStringLiteral("url")).toString();
                const QString fileName = file.value(QStringLiteral("filename")).toString();
                if (fileUrl.isEmpty() || fileName.isEmpty()) {
                    continue;
                }

                auto *versionButton = new QPushButton(items);
                versionButton->setCursor(Qt::PointingHandCursor);
                versionButton->setStyleSheet(QStringLiteral(
                    "QPushButton { background: transparent; border: none; border-radius: 8px;"
                    "text-align: left; padding: 7px; }"
                    "QPushButton:hover { background: rgba(110, 231, 183, 14); }"));
                versionButton->setMinimumHeight(52);
                auto *versionLayout = new QHBoxLayout(versionButton);
                versionLayout->setContentsMargins(5, 2, 5, 2);
                versionLayout->setSpacing(10);

                auto *versionText = new QVBoxLayout();
                versionText->setContentsMargins(0, 0, 0, 0);
                versionText->setSpacing(2);
                const QString number =
                    version.value(QStringLiteral("version_number")).toString();
                versionText->addWidget(Theme::label(
                    number.isEmpty() ? QStringLiteral("未命名版本") : number,
                    11, QFont::DemiBold, Theme::TextPrimary, versionButton));
                QJsonArray loaders = version.value(QStringLiteral("loaders")).toArray();
                QStringList loaderNames;
                for (const QJsonValue &loader : loaders) {
                    loaderNames.append(prettyLoader(loader.toString()));
                }
                const QString details = QStringLiteral("%1  ·  更新于 %2")
                    .arg(loaderNames.join(QStringLiteral(" / ")),
                         formattedDate(version.value(QStringLiteral("date_published")).toString()));
                versionText->addWidget(mutedLabel(details, versionButton));
                versionLayout->addLayout(versionText, 1);
                versionLayout->addWidget(mutedLabel(
                    QStringLiteral("%1 次下载").arg(formattedDownloads(
                        version.value(QStringLiteral("downloads")).toVariant().toLongLong())),
                    versionButton));
                itemsLayout->addWidget(versionButton);

                const QByteArray sha1 =
                    file.value(QStringLiteral("hashes")).toObject()
                        .value(QStringLiteral("sha1")).toString().toLatin1();
                const QString targetName = fileName;
                const QUrl targetUrl(fileUrl);
                const QString versionId = gameVersionId;
                QObject::connect(versionButton, &QPushButton::clicked, dialog,
                                 [dialog, gameDirectory, versionId, targetName,
                                  targetUrl, sha1, loaderFilter, download]() {
                    QString startDirectory =
                        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
                    if (startDirectory.isEmpty()) {
                        startDirectory = QDir::homePath();
                    }
                    QString createdModsDirectory;
                    QString modsDirectoryError;
                    if (!gameDirectory.isEmpty() && versionId != QStringLiteral("其他版本")) {
                        const LocalVersionScanner::Result scan =
                            LocalVersionScanner::scan(gameDirectory);
                        for (const VersionInfo &installed : scan.versions) {
                            if (installed.minecraft == versionId
                                && (loaderFilter.isEmpty()
                                    || installed.loader.compare(
                                           loaderFilter, Qt::CaseInsensitive) == 0)) {
                                const QString gameData = installed.gameDataDirectory.isEmpty()
                                    ? gameDirectory
                                    : installed.gameDataDirectory;
                                const QString installedMods =
                                    QDir(gameData).filePath(QStringLiteral("mods"));
                                const bool modsDirectoryExists = QDir(installedMods).exists();
                                if (QDir().mkpath(installedMods)) {
                                    startDirectory = installedMods;
                                    if (!modsDirectoryExists) {
                                        createdModsDirectory = installedMods;
                                    }
                                } else {
                                    modsDirectoryError = installedMods;
                                }
                                break;
                            }
                        }
                    }
                    if (!modsDirectoryError.isEmpty()) {
                        QMessageBox::warning(
                            dialog, QStringLiteral("无法打开模组目录"),
                            QStringLiteral("无法创建或访问该游戏版本的 mods 目录：\n%1")
                                .arg(modsDirectoryError));
                        return;
                    }

                    const QString suggested =
                        QDir(startDirectory).filePath(targetName);
                    const QString destination = QFileDialog::getSaveFileName(
                        dialog, QStringLiteral("选择模组保存位置"), suggested,
                        QStringLiteral("模组文件 (*.jar);;所有文件 (*)"));
                    if (destination.isEmpty()) {
                        if (!createdModsDirectory.isEmpty()) {
                            QDir().rmdir(createdModsDirectory);
                        }
                        return;
                    }
                    QString destinationPath = destination;
                    if (!destinationPath.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
                        destinationPath += QStringLiteral(".jar");
                    }
                    if (QFileInfo::exists(destinationPath)
                        && QMessageBox::question(
                               dialog, QStringLiteral("确认覆盖"),
                               QStringLiteral("文件已存在，是否覆盖？\n%1")
                                   .arg(destinationPath),
                               QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::No) != QMessageBox::Yes) {
                        if (!createdModsDirectory.isEmpty()) {
                            QDir().rmdir(createdModsDirectory);
                        }
                        return;
                    }
                    if (download(targetUrl, destinationPath, sha1, targetName)) {
                        dialog->close();
                    }
                });
            }

            items->setVisible(expanded);
            groupLayout->addWidget(items);
            QObject::connect(toggle, &QToolButton::toggled, group,
                             [items, toggle](bool expanded) {
                items->setVisible(expanded);
                toggle->setArrowType(expanded ? Qt::UpArrow : Qt::DownArrow);
            });
            targetLayout->addWidget(group);
        };

        if (!preferredGameVersion.isEmpty() || !preferredLoader.isEmpty()) {
            QStringList recommendedKeys;
            if (!preferredGameVersion.isEmpty()) {
                if (groups.contains(preferredGameVersion)) {
                    recommendedKeys.append(preferredGameVersion);
                }
            } else {
                for (const QString &gameVersionId : gameVersionKeys) {
                    if (gameVersionId == QStringLiteral("其他版本")) {
                        continue;
                    }
                    if (preferredLoader.isEmpty()
                        || hasLoader(gameVersionId, preferredLoader)) {
                        recommendedKeys.append(gameVersionId);
                    }
                }
            }

            int renderedRecommendations = 0;
            for (const QString &gameVersionId : recommendedKeys) {
                QVector<QJsonObject> recommendedVersions = groups.value(gameVersionId);
                if (!preferredLoader.isEmpty()) {
                    recommendedVersions.erase(
                        std::remove_if(recommendedVersions.begin(),
                                       recommendedVersions.end(),
                                       [&preferredLoader](const QJsonObject &version) {
                            for (const QJsonValue &loader :
                                 version.value(QStringLiteral("loaders")).toArray()) {
                                if (loader.toString() == preferredLoader) {
                                    return false;
                                }
                            }
                            return true;
                        }),
                        recommendedVersions.end());
                }
                if (recommendedVersions.isEmpty()) {
                    continue;
                }

                QString groupTitle = gameVersionId;
                if (!preferredLoader.isEmpty()) {
                    groupTitle += QStringLiteral(" · %1").arg(prettyLoader(preferredLoader));
                }
                renderVersionGroup(recommendationLayout, gameVersionId, groupTitle,
                                   recommendedVersions, preferredLoader,
                                   renderedRecommendations == 0);
                ++renderedRecommendations;
            }
            if (renderedRecommendations == 0) {
                recommendationLayout->addWidget(mutedLabel(
                    QStringLiteral("当前筛选条件下没有可直接推荐的版本，请在下方完整列表中查看兼容版本。"),
                    dialog));
            }
            recommendationLayout->addStretch(1);
        }

        for (const QString &gameVersionId : gameVersionKeys) {
            renderVersionGroup(versionsLayout, gameVersionId, gameVersionId,
                               groups.value(gameVersionId), QString(), false);
        }
        versionsLayout->addStretch(1);
    });
}

} // namespace

DownloadModsPage::DownloadModsPage(QWidget *parent)
    : QWidget(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_gameDirectory(MinecraftPaths::savedGameDirectory())
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 22, 32, 16);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);
    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(3);
    auto *eyebrow = Theme::label(QStringLiteral("模组库  /  MODRINTH"), 9, QFont::Bold,
                                 Theme::Accent, this);
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    eyebrow->setFont(eyebrowFont);
    headerText->addWidget(eyebrow);
    headerText->addWidget(Theme::label(QStringLiteral("下载模组"), 27, QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    headerText->addWidget(Theme::label(QStringLiteral("搜索 Modrinth 模组并选择兼容版本下载。"),
                                       12, QFont::Normal, Theme::TextMuted, this));
    header->addLayout(headerText);
    header->addStretch();
    header->addWidget(Theme::badge(QStringLiteral("数据来源：Modrinth"),
                                   Theme::Accent, QColor(Theme::Accent.red(),
                                                         Theme::Accent.green(),
                                                         Theme::Accent.blue(), 18),
                                   this, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                                Theme::Accent.blue(), 45)),
                      0, Qt::AlignVCenter);
    root->addLayout(header);
    root->addSpacing(14);

    auto *downloadPanel = new CardWidget(this);
    downloadPanel->setAccentColor(Theme::Accent);
    downloadPanel->setCornerRadius(14);
    m_downloadPanel = downloadPanel;
    auto *downloadLayout = new QVBoxLayout(m_downloadPanel);
    downloadLayout->setContentsMargins(16, 10, 16, 10);
    downloadLayout->setSpacing(6);
    auto *downloadTop = new QHBoxLayout();
    downloadTop->setContentsMargins(0, 0, 0, 0);
    m_downloadName = Theme::label(QString(), 11, QFont::DemiBold,
                                  Theme::TextPrimary, m_downloadPanel);
    m_downloadSpeed = mutedLabel(QString(), m_downloadPanel);
    downloadTop->addWidget(m_downloadName, 1);
    downloadTop->addWidget(m_downloadSpeed);
    m_cancelModDownloadButton = new QPushButton(QStringLiteral("取消下载"),
                                                m_downloadPanel);
    m_cancelModDownloadButton->setCursor(Qt::PointingHandCursor);
    m_cancelModDownloadButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        "border-radius: 7px; padding: 4px 9px; }"
        "QPushButton:hover:enabled { background: rgba(110,231,183,16); }"
        "QPushButton:disabled { color: %3; }")
        .arg(Theme::TextPrimary.name(), Theme::Border.name(), Theme::TextMuted.name()));
    m_cancelModDownloadButton->hide();
    downloadTop->addWidget(m_cancelModDownloadButton);
    downloadLayout->addLayout(downloadTop);
    m_downloadProgress = new QProgressBar(m_downloadPanel);
    m_downloadProgress->setRange(0, 100);
    Theme::setProgressValue(m_downloadProgress, 0);
    m_downloadProgress->setTextVisible(false);
    downloadLayout->addWidget(m_downloadProgress);
    m_downloadPanel->hide();
    root->addWidget(m_downloadPanel);
    root->addSpacing(10);

    auto *filterCard = new CardWidget(this);
    filterCard->setAccentColor(Theme::Blue);
    filterCard->setInteractive(false);
    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(18, 14, 18, 14);
    filterLayout->setSpacing(10);
    filterLayout->addWidget(Theme::label(QStringLiteral("搜索 Mod"), 15, QFont::DemiBold,
                                         Theme::TextPrimary, filterCard));

    auto *fields = new QGridLayout();
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setHorizontalSpacing(12);
    fields->setVerticalSpacing(8);
    auto *nameLabel = Theme::label(QStringLiteral("名称"), 11, QFont::DemiBold,
                                   Theme::TextSecondary, filterCard);
    m_nameEdit = new QLineEdit(filterCard);
    m_nameEdit->setPlaceholderText(QStringLiteral("输入模组名称或关键词"));
    m_nameEdit->setClearButtonEnabled(true);
    m_nameEdit->setFixedHeight(40);
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: transparent; border: 1px solid %1; border-radius: 9px;"
        "padding: 6px 10px; color: %2; }"
        "QLineEdit:hover { background: rgba(255,255,255,8); border-color: %3; }"
        "QLineEdit:focus { background: transparent; border-color: %4; }")
        .arg(Theme::Border.name(), Theme::TextPrimary.name(),
             QColor(255, 255, 255, 64).name(QColor::HexArgb),
             Theme::Accent.name()));
    auto *versionLabel = Theme::label(QStringLiteral("游戏版本"), 11, QFont::DemiBold,
                                      Theme::TextSecondary, filterCard);
    m_gameVersionCombo = new AnimatedComboBox(filterCard);
    m_gameVersionCombo->setEditable(true);
    m_gameVersionCombo->setInsertPolicy(QComboBox::NoInsert);
    m_gameVersionCombo->setPlaceholderText(QStringLiteral("全部（也可自行输入）"));
    m_gameVersionCombo->addItem(QStringLiteral("全部"));
    m_gameVersionCombo->setFixedHeight(40);
    auto *loaderLabel = Theme::label(QStringLiteral("加载器"), 11, QFont::DemiBold,
                                     Theme::TextSecondary, filterCard);
    m_loaderCombo = new AnimatedComboBox(filterCard);
    m_loaderCombo->addItem(QStringLiteral("任意 Mod 加载器"), QString());
    m_loaderCombo->addItem(QStringLiteral("Forge"), QStringLiteral("forge"));
    m_loaderCombo->addItem(QStringLiteral("NeoForge"), QStringLiteral("neoforge"));
    m_loaderCombo->addItem(QStringLiteral("Fabric"), QStringLiteral("fabric"));
    m_loaderCombo->addItem(QStringLiteral("LiteLoader"), QStringLiteral("liteloader"));
    m_loaderCombo->setFixedHeight(40);

    const QString comboStyle = QStringLiteral(
        "QComboBox { background: transparent; border: 1px solid %1; border-radius: 9px;"
        "padding: 6px 10px; color: %2; }"
        "QComboBox:hover { background: rgba(255,255,255,8); border-color: %3; }"
        "QComboBox:focus { background: transparent; border-color: %4; }"
        "QComboBox QAbstractItemView { background: %5; color: %2; border: 1px solid %1;"
        "selection-background-color: %6; selection-color: %2; outline: 0; }")
        .arg(Theme::Border.name(), Theme::TextPrimary.name(),
             QColor(255, 255, 255, 64).name(QColor::HexArgb),
             Theme::Accent.name(), Theme::SurfaceRaised.name(),
             QColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(), 56)
                 .name(QColor::HexArgb));
    m_gameVersionCombo->setStyleSheet(comboStyle);
    m_gameVersionCombo->lineEdit()->setStyleSheet(
        QStringLiteral("QLineEdit { background: transparent; border: none; padding: 0; }"));
    m_loaderCombo->setStyleSheet(comboStyle);
    nameLabel->setFixedWidth(72);
    versionLabel->setFixedWidth(72);
    loaderLabel->setFixedWidth(72);
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    versionLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    loaderLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    fields->addWidget(nameLabel, 0, 0);
    fields->addWidget(m_nameEdit, 0, 1);
    fields->addWidget(versionLabel, 1, 0);
    fields->addWidget(m_gameVersionCombo, 1, 1, 1, 3);
    fields->addWidget(loaderLabel, 0, 2);
    fields->addWidget(m_loaderCombo, 0, 3);
    fields->setColumnMinimumWidth(0, 72);
    fields->setColumnMinimumWidth(2, 72);
    fields->setColumnStretch(1, 1);
    fields->setColumnStretch(3, 1);
    filterLayout->addLayout(fields);

    auto *versionLoadingRow = new QWidget(filterCard);
    auto *versionLoadingLayout = new QHBoxLayout(versionLoadingRow);
    versionLoadingLayout->setContentsMargins(84, 0, 0, 0);
    versionLoadingLayout->setSpacing(7);
    m_versionLoadingIndicator = new LoadingIndicator(versionLoadingRow, Theme::Accent);
    m_versionLoadingIndicator->setFixedSize(17, 17);
    versionLoadingLayout->addWidget(m_versionLoadingIndicator);
    m_versionLoadingLabel = mutedLabel(
        QStringLiteral("正在获取 Minecraft 版本清单…"), versionLoadingRow);
    versionLoadingLayout->addWidget(m_versionLoadingLabel);
    m_retryVersionManifestButton = new QPushButton(QStringLiteral("重试"),
                                                    versionLoadingRow);
    m_retryVersionManifestButton->setCursor(Qt::PointingHandCursor);
    m_retryVersionManifestButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        "border-radius: 7px; padding: 3px 8px; }"
        "QPushButton:hover { background: rgba(110,231,183,16); }")
        .arg(Theme::TextPrimary.name(), Theme::Border.name()));
    m_retryVersionManifestButton->hide();
    versionLoadingLayout->addWidget(m_retryVersionManifestButton);
    versionLoadingLayout->addStretch();
    filterLayout->addWidget(versionLoadingRow);

    auto *actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(8);
    auto *searchButton = new AccentButton(QStringLiteral("搜索"), this);
    searchButton->setVariant(AccentButton::Variant::Primary);
    searchButton->setFixedWidth(130);
    auto *resetButton = new AccentButton(QStringLiteral("重置条件"), this);
    resetButton->setVariant(AccentButton::Variant::Secondary);
    resetButton->setFixedWidth(130);
    actions->addWidget(searchButton);
    actions->addWidget(resetButton);
    actions->addStretch();
    filterLayout->addLayout(actions);
    root->addWidget(filterCard);
    root->addSpacing(10);

    auto *resultHeader = new QHBoxLayout();
    resultHeader->setContentsMargins(2, 0, 2, 0);
    resultHeader->addWidget(Theme::label(QStringLiteral("搜索结果"), 14, QFont::DemiBold,
                                         Theme::TextPrimary, this));
    m_countLabel = mutedLabel(QString(), this);
    resultHeader->addWidget(m_countLabel);
    resultHeader->addStretch();
    m_searchStatus = mutedLabel(QStringLiteral("设置筛选条件后点击“搜索”。"), this);
    resultHeader->addWidget(m_searchStatus);
    root->addLayout(resultHeader);
    root->addSpacing(7);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    m_resultsContent = new QWidget();
    m_resultsContent->setAttribute(Qt::WA_NoSystemBackground);
    m_resultsLayout = new QVBoxLayout(m_resultsContent);
    m_resultsLayout->setContentsMargins(0, 0, 7, 4);
    m_resultsLayout->setSpacing(6);
    m_resultsLayout->addStretch(1);
    m_loadingWidget = new QWidget(m_resultsContent);
    auto *loadingLayout = new QVBoxLayout(m_loadingWidget);
    loadingLayout->setContentsMargins(0, 20, 0, 20);
    loadingLayout->setSpacing(10);
    loadingLayout->addStretch(1);
    auto *loadingIndicator = new LoadingIndicator(m_loadingWidget, Theme::Accent);
    loadingIndicator->setFixedSize(30, 30);
    loadingLayout->addWidget(loadingIndicator, 0, Qt::AlignHCenter);
    auto *loadingLabel = mutedLabel(QStringLiteral("正在从 Modrinth 加载模组…"),
                                    m_loadingWidget);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    loadingLayout->addStretch(1);
    m_loadingWidget->hide();
    scroll->setWidget(m_resultsContent);
    root->addWidget(scroll, 1);

    auto *pagination = new QHBoxLayout();
    pagination->setContentsMargins(2, 7, 7, 0);
    pagination->setSpacing(8);
    pagination->addStretch();
    m_previousPageButton = new QPushButton(QStringLiteral("上一页"), this);
    m_previousPageButton->setCursor(Qt::PointingHandCursor);
    m_previousPageButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        "border-radius: 9px; padding: 7px 14px; }"
        "QPushButton:hover:enabled { background: rgba(110,231,183,16); }"
        "QPushButton:disabled { color: %3; border-color: %4; }")
        .arg(Theme::TextPrimary.name(), Theme::Border.name(),
             Theme::TextMuted.name(), Theme::Border.name()));
    m_pageLabel = mutedLabel(QString(), this);
    m_nextPageButton = new QPushButton(QStringLiteral("下一页"), this);
    m_nextPageButton->setCursor(Qt::PointingHandCursor);
    m_nextPageButton->setStyleSheet(m_previousPageButton->styleSheet());
    pagination->addWidget(m_previousPageButton);
    pagination->addWidget(m_pageLabel);
    pagination->addWidget(m_nextPageButton);
    m_previousPageButton->hide();
    m_pageLabel->hide();
    m_nextPageButton->hide();
    root->addLayout(pagination);

    connect(searchButton, &QAbstractButton::clicked,
            this, &DownloadModsPage::searchProjects);
    connect(m_nameEdit, &QLineEdit::returnPressed,
            this, &DownloadModsPage::searchProjects);
    const auto invalidateResults = [this]() {
        if (m_totalHits == 0) {
            return;
        }
        m_currentPage = 0;
        m_totalHits = 0;
        m_countLabel->clear();
        m_previousPageButton->hide();
        m_pageLabel->hide();
        m_nextPageButton->hide();
        setSearchStatus(QStringLiteral("筛选条件已修改，请重新搜索。"));
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this,
            [invalidateResults](const QString &) { invalidateResults(); });
    connect(m_gameVersionCombo, &QComboBox::currentTextChanged, this,
            [invalidateResults](const QString &) { invalidateResults(); });
    connect(m_loaderCombo, &QComboBox::currentIndexChanged, this,
            [invalidateResults](int) { invalidateResults(); });
    connect(resetButton, &QAbstractButton::clicked, this, [this]() {
        if (m_searchReply) {
            disconnect(m_searchReply, nullptr, this, nullptr);
            m_searchReply->abort();
            m_searchReply->deleteLater();
            m_searchReply = nullptr;
        }
        clearResults();
        m_nameEdit->clear();
        m_gameVersionCombo->setCurrentText(QStringLiteral("全部"));
        m_loaderCombo->setCurrentIndex(0);
        m_currentPage = 0;
        m_totalHits = 0;
        m_countLabel->clear();
        setSearchStatus(QStringLiteral("筛选条件已重置，请点击“搜索”。"));
        m_resultsLayout->addStretch(1);
        m_previousPageButton->hide();
        m_pageLabel->hide();
        m_nextPageButton->hide();
    });

    connect(m_previousPageButton, &QPushButton::clicked, this, [this]() {
        if (m_currentPage > 0) {
            --m_currentPage;
            fetchSearchPage();
        }
    });
    connect(m_nextPageButton, &QPushButton::clicked, this, [this]() {
        if ((m_currentPage + 1) * kProjectsPerPage < m_totalHits) {
            ++m_currentPage;
            fetchSearchPage();
        }
    });

    m_manifestClient = new VersionManifestClient(this);
    connect(m_manifestClient, &VersionManifestClient::ready, this,
            [this](const MinecraftVersionManifest &result) {
        m_versionLoadingIndicator->hide();
        m_versionLoadingLabel->hide();
        m_retryVersionManifestButton->hide();
        const QString typedText = m_gameVersionCombo->currentText();
        m_gameVersionIds.clear();
        m_gameVersionCombo->clear();
        m_gameVersionCombo->addItem(QStringLiteral("全部"));
        for (const MinecraftVersionEntry &version : result.versions) {
            m_gameVersionIds.append(version.id);
            m_gameVersionCombo->addItem(version.id);
        }
        m_gameVersionCombo->setCurrentText(typedText);
    });
    connect(m_manifestClient, &VersionManifestClient::failed, this,
            [this](const QString &reason) {
        m_versionLoadingIndicator->hide();
        m_versionLoadingLabel->setText(QStringLiteral("Minecraft 版本清单加载失败。"));
        m_retryVersionManifestButton->show();
        setSearchStatus(QStringLiteral("无法加载 Minecraft 版本清单：%1").arg(reason));
        m_pendingLoadError =
            QStringLiteral("无法加载 Minecraft 版本清单：%1").arg(reason);
        if (isVisible()) {
            QMessageBox::warning(this, QStringLiteral("版本清单加载失败"),
                                 m_pendingLoadError);
            m_pendingLoadError.clear();
        }
    });
    connect(m_retryVersionManifestButton, &QPushButton::clicked, this, [this]() {
        m_versionLoadingIndicator->show();
        m_versionLoadingLabel->setText(
            QStringLiteral("正在获取 Minecraft 版本清单…"));
        m_versionLoadingLabel->show();
        m_retryVersionManifestButton->hide();
        m_manifestClient->fetch();
    });
}

void DownloadModsPage::setGameDirectory(const QString &directory)
{
    const QString trimmed = directory.trimmed();
    if (!trimmed.isEmpty()) {
        m_gameDirectory = QDir(trimmed).absolutePath();
    }
}

void DownloadModsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_pendingLoadError.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), m_pendingLoadError);
        m_pendingLoadError.clear();
    }
    if (!m_initialSearchStarted) {
        m_initialSearchStarted = true;
        if (m_manifestClient && !m_manifestClient->isFetching()
            && m_gameVersionIds.isEmpty() && m_pendingLoadError.isEmpty()) {
            m_versionLoadingIndicator->show();
            m_versionLoadingLabel->setText(
                QStringLiteral("正在获取 Minecraft 版本清单…"));
            m_versionLoadingLabel->show();
            m_manifestClient->fetch();
        }
        searchProjects();
    }
}

void DownloadModsPage::setSearchStatus(const QString &message)
{
    if (m_searchStatus) {
        m_searchStatus->setText(message);
    }
}

void DownloadModsPage::clearResults()
{
    while (m_resultsLayout->count() > 0) {
        QLayoutItem *item = m_resultsLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            if (widget == m_loadingWidget) {
                widget->hide();
            } else {
                widget->deleteLater();
            }
        }
        delete item;
    }
}

void DownloadModsPage::searchProjects()
{
    QString gameVersion = m_gameVersionCombo->currentText().trimmed();
    if (gameVersion == QStringLiteral("全部")) {
        gameVersion.clear();
    }
    if (!gameVersion.isEmpty()) {
        if (m_gameVersionIds.isEmpty()) {
            setSearchStatus(QStringLiteral("Minecraft 版本清单仍在加载，请稍后重试。"));
            return;
        }
        if (!m_gameVersionIds.contains(gameVersion)) {
            setSearchStatus(QStringLiteral("Minecraft 版本“%1”不存在，请检查输入。")
                                .arg(gameVersion));
            return;
        }
    }

    m_currentPage = 0;
    m_totalHits = 0;
    fetchSearchPage();
}

void DownloadModsPage::fetchSearchPage()
{
    if (m_searchReply) {
        disconnect(m_searchReply, nullptr, this, nullptr);
        m_searchReply->abort();
        m_searchReply->deleteLater();
        m_searchReply = nullptr;
    }

    m_previousPageButton->setEnabled(false);
    m_nextPageButton->setEnabled(false);
    m_previousPageButton->hide();
    m_pageLabel->hide();
    m_nextPageButton->hide();

    QUrl url(kModrinthApi.toString() + QStringLiteral("/search"));
    QUrlQuery query;
    const QString name = m_nameEdit->text().trimmed();
    QString gameVersion = m_gameVersionCombo->currentText().trimmed();
    if (gameVersion == QStringLiteral("全部")) {
        gameVersion.clear();
    }
    if (!name.isEmpty()) {
        query.addQueryItem(QStringLiteral("query"), name);
    }

    QJsonArray facets;
    facets.append(QJsonArray{QStringLiteral("project_type:mod")});
    if (!gameVersion.isEmpty()) {
        facets.append(QJsonArray{QStringLiteral("versions:%1").arg(gameVersion)});
    }
    const QString loader = m_loaderCombo->currentData().toString();
    if (!loader.isEmpty()) {
        facets.append(QJsonArray{QStringLiteral("categories:%1").arg(loader)});
    }
    query.addQueryItem(QStringLiteral("facets"),
                       QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    query.addQueryItem(QStringLiteral("limit"), QString::number(kProjectsPerPage));
    query.addQueryItem(QStringLiteral("offset"),
                       QString::number(m_currentPage * kProjectsPerPage));
    query.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(apiRequest(url));
    if (!reply) {
        clearResults();
        m_resultsLayout->addStretch(1);
        const QString message = QStringLiteral("无法发起 Modrinth 搜索请求。");
        setSearchStatus(message);
        QMessageBox::warning(this, QStringLiteral("模组搜索失败"), message);
        return;
    }
    m_searchReply = reply;
    m_countLabel->clear();
    clearResults();
    m_resultsLayout->addStretch(1);
    m_resultsLayout->addWidget(m_loadingWidget, 0, Qt::AlignCenter);
    m_resultsLayout->addStretch(1);
    m_loadingWidget->show();
    setSearchStatus(QStringLiteral("正在加载第 %1 页…").arg(m_currentPage + 1));
    connect(reply, &QNetworkReply::finished,
            this, &DownloadModsPage::handleSearchReply);
}

void DownloadModsPage::handleSearchReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_searchReply) {
        return;
    }
    m_searchReply = nullptr;
    reply->deleteLater();

    const auto showFailure = [this](const QString &message) {
        clearResults();
        m_resultsLayout->addStretch(1);
        setSearchStatus(message);
        QMessageBox::warning(this, QStringLiteral("模组搜索失败"), message);
    };

    if (reply->error() != QNetworkReply::NoError) {
        showFailure(QStringLiteral("搜索失败：%1").arg(reply->errorString()));
        return;
    }
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        showFailure(QStringLiteral("搜索失败：Modrinth 返回 HTTP %1").arg(status));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        showFailure(QStringLiteral("搜索失败：Modrinth 返回了无效数据。"));
        return;
    }

    const QJsonArray hits = document.object().value(QStringLiteral("hits")).toArray();
    m_totalHits =
        document.object().value(QStringLiteral("total_hits")).toInt(hits.size());
    clearResults();

    int displayed = 0;
    for (const QJsonValue &value : hits) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject project = value.toObject();
        const QString projectId = project.value(QStringLiteral("project_id")).toString();
        const QString title = project.value(QStringLiteral("title")).toString();
        if (projectId.isEmpty() || title.isEmpty()) {
            continue;
        }

        auto *row = new QPushButton(m_resultsContent);
        row->setCursor(Qt::PointingHandCursor);
        row->setFlat(true);
        row->setMinimumHeight(100);
        row->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; border-bottom: 1px solid %1;"
            "text-align: left; padding: 10px 8px; }"
            "QPushButton:hover { background: rgba(110, 231, 183, 12); }")
            .arg(Theme::Border.name()));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(4, 3, 8, 3);
        layout->setSpacing(12);

        auto *icon = new QLabel(row);
        icon->setFixedSize(64, 64);
        icon->setAlignment(Qt::AlignCenter);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        icon->setStyleSheet(QStringLiteral("background: rgba(255,255,255,12);"
                                           "border-radius: 10px;"));
        layout->addWidget(icon, 0, Qt::AlignVCenter);
        setRemoteIcon(m_network, project.value(QStringLiteral("icon_url")).toString(),
                      icon, QSize(64, 64));

        auto *text = new QVBoxLayout();
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(3);
        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(8);
        titleRow->addWidget(Theme::label(title, 14, QFont::DemiBold,
                                         Theme::TextPrimary, row));
        const QString slug = project.value(QStringLiteral("slug")).toString();
        if (!slug.isEmpty() && slug.compare(title, Qt::CaseInsensitive) != 0) {
            titleRow->addWidget(mutedLabel(slug, row));
        }
        titleRow->addStretch();
        titleRow->addWidget(mutedLabel(QStringLiteral("Modrinth"), row));
        text->addLayout(titleRow);

        auto *description = mutedLabel(
            project.value(QStringLiteral("description")).toString(), row);
        description->setWordWrap(false);
        text->addWidget(description);

        QStringList categories;
        for (const QJsonValue &category : project.value(QStringLiteral("categories")).toArray()) {
            categories.append(prettyLoader(category.toString()));
        }
        const QString versionText = project.value(QStringLiteral("versions")).toArray().isEmpty()
            ? QStringLiteral("未标注游戏版本")
            : project.value(QStringLiteral("versions")).toArray().first().toString()
                + (project.value(QStringLiteral("versions")).toArray().size() > 1
                       ? QStringLiteral(" 等")
                       : QString());
        const QString meta = QStringLiteral("%1  ·  %2  ·  下载 %3  ·  更新于 %4")
            .arg(categories.isEmpty() ? QStringLiteral("加载器未标注")
                                      : categories.join(QStringLiteral(" / ")),
                 versionText,
                 formattedDownloads(project.value(QStringLiteral("downloads"))
                                        .toVariant().toLongLong()),
                 formattedDate(project.value(QStringLiteral("date_modified")).toString()));
        text->addWidget(mutedLabel(meta, row));
        layout->addLayout(text, 1);

        connect(row, &QPushButton::clicked, this,
                [this, project]() { showProjectDetails(project); });
        m_resultsLayout->addWidget(row);
        ++displayed;
    }
    m_resultsLayout->addStretch(1);
    const int totalPages = (m_totalHits + kProjectsPerPage - 1) / kProjectsPerPage;
    m_countLabel->setText(QStringLiteral("共 %1 个模组").arg(m_totalHits));
    m_pageLabel->setText(
        QStringLiteral("第 %1 / %2 页").arg(m_currentPage + 1).arg(qMax(1, totalPages)));
    const bool multiplePages = totalPages > 1;
    m_previousPageButton->setEnabled(m_currentPage > 0);
    m_nextPageButton->setEnabled(m_currentPage + 1 < totalPages);
    m_previousPageButton->setVisible(multiplePages);
    m_pageLabel->setVisible(multiplePages);
    m_nextPageButton->setVisible(multiplePages);
    setSearchStatus(displayed == 0
                        ? QStringLiteral("没有找到匹配的模组，请尝试调整筛选条件。")
                        : QStringLiteral("点击列表项查看详情和可下载版本。"));
}

void DownloadModsPage::showProjectDetails(const QJsonObject &project)
{
    const QString projectId = project.value(QStringLiteral("project_id")).toString();
    const QString title = project.value(QStringLiteral("title")).toString();
    if (projectId.isEmpty() || title.isEmpty()) {
        emit notifyRequested(QStringLiteral("模组信息缺失，无法打开详情。"));
        return;
    }
    QString preferredGameVersion = m_gameVersionCombo->currentText().trimmed();
    if (preferredGameVersion == QStringLiteral("全部")) {
        preferredGameVersion.clear();
    }
    const QString preferredLoader = m_loaderCombo->currentData().toString();

    auto *dialog = new ModDetailsDialog(window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setWindowTitle(title);
    dialog->resize(900, 720);
    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(10);

    auto *summary = new CardWidget(dialog);
    summary->setInteractive(false);
    auto *summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    summaryLayout->setSpacing(10);

    auto *summaryTop = new QHBoxLayout();
    summaryTop->setContentsMargins(0, 0, 0, 0);
    summaryTop->setSpacing(12);
    auto *icon = new QLabel(summary);
    icon->setFixedSize(72, 72);
    icon->setAlignment(Qt::AlignCenter);
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon->setStyleSheet(QStringLiteral("background: rgba(255,255,255,12);"
                                       "border-radius: 12px;"));
    summaryTop->addWidget(icon);
    setRemoteIcon(m_network, project.value(QStringLiteral("icon_url")).toString(),
                  icon, QSize(72, 72));

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(3);
    titleBlock->addWidget(Theme::label(title, 20, QFont::DemiBold,
                                       Theme::TextPrimary, summary));
    const QString slug = project.value(QStringLiteral("slug")).toString();
    titleBlock->addWidget(mutedLabel(
        slug.isEmpty()
            ? QStringLiteral("Modrinth 项目")
            : QStringLiteral("英文名 / Modrinth ID：%1").arg(slug),
        summary));
    titleBlock->addWidget(mutedLabel(
        project.value(QStringLiteral("description")).toString(), summary));
    summaryTop->addLayout(titleBlock, 1);
    summaryLayout->addLayout(summaryTop);

    QStringList loaders;
    for (const QJsonValue &category : project.value(QStringLiteral("categories")).toArray()) {
        const QString value = category.toString();
        if (value == QStringLiteral("forge") || value == QStringLiteral("neoforge")
            || value == QStringLiteral("fabric") || value == QStringLiteral("liteloader")) {
            loaders.append(prettyLoader(value));
        }
    }
    const QString loaderText = loaders.isEmpty()
        ? QStringLiteral("项目未标注")
        : loaders.join(QStringLiteral(" / "));
    auto *metadata = new QHBoxLayout();
    metadata->setContentsMargins(0, 0, 0, 0);
    metadata->setSpacing(22);
    metadata->addWidget(metadataItem(QStringLiteral("加载器"), loaderText, summary));
    metadata->addWidget(metadataItem(
        QStringLiteral("下载量"),
        formattedDownloads(project.value(QStringLiteral("downloads")).toVariant().toLongLong()),
        summary));
    metadata->addWidget(metadataItem(
        QStringLiteral("上传/更新"),
        formattedDate(project.value(QStringLiteral("date_modified")).toString()), summary));
    metadata->addStretch();
    summaryLayout->addLayout(metadata);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);
    auto *modrinthButton = new AccentButton(QStringLiteral("转到 Modrinth"),
                                             Icons::Type::External, summary);
    modrinthButton->setVariant(AccentButton::Variant::Secondary);
    const QString projectUrl = QStringLiteral("https://modrinth.com/mod/")
        + (slug.isEmpty() ? projectId : slug);
    connect(modrinthButton, &QAbstractButton::clicked, dialog,
            [projectUrl]() { QDesktopServices::openUrl(QUrl(projectUrl)); });
    buttons->addWidget(modrinthButton);
    buttons->addStretch();
    summaryLayout->addLayout(buttons);
    root->addWidget(summary);

    auto *recommendationCard = new CardWidget(dialog);
    recommendationCard->setInteractive(false);
    recommendationCard->setAccentColor(Theme::Accent);
    recommendationCard->setCornerRadius(14);
    recommendationCard->setVisible(!preferredGameVersion.isEmpty()
                                   || !preferredLoader.isEmpty());
    auto *recommendationCardLayout = new QVBoxLayout(recommendationCard);
    recommendationCardLayout->setContentsMargins(14, 11, 14, 11);
    recommendationCardLayout->setSpacing(6);
    auto *recommendationHeader = new QHBoxLayout();
    recommendationHeader->setContentsMargins(0, 0, 0, 0);
    recommendationHeader->setSpacing(8);
    recommendationHeader->addWidget(Theme::label(
        QStringLiteral("推荐下载"), 14, QFont::DemiBold,
        Theme::Accent, recommendationCard));
    recommendationHeader->addStretch();
    if (!preferredGameVersion.isEmpty()) {
        recommendationHeader->addWidget(Theme::badge(
            QStringLiteral("Minecraft %1").arg(preferredGameVersion),
            Theme::Accent, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                  Theme::Accent.blue(), 20),
            recommendationCard,
            QColor(Theme::Accent.red(), Theme::Accent.green(),
                   Theme::Accent.blue(), 55)));
    }
    if (!preferredLoader.isEmpty()) {
        recommendationHeader->addWidget(Theme::badge(
            prettyLoader(preferredLoader), Theme::Blue,
            QColor(Theme::Blue.red(), Theme::Blue.green(), Theme::Blue.blue(), 20),
            recommendationCard,
            QColor(Theme::Blue.red(), Theme::Blue.green(), Theme::Blue.blue(), 55)));
    }
    recommendationCardLayout->addLayout(recommendationHeader);
    recommendationCardLayout->addWidget(mutedLabel(
        QStringLiteral("以下版本根据当前筛选条件整理；下方仍保留全部兼容版本。"),
        recommendationCard));

    auto *recommendationScroll = new QScrollArea(recommendationCard);
    recommendationScroll->setWidgetResizable(true);
    recommendationScroll->setFrameShape(QFrame::NoFrame);
    recommendationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recommendationScroll->setMaximumHeight(116);
    recommendationScroll->viewport()->setAutoFillBackground(false);
    auto *recommendationContent = new QWidget();
    recommendationContent->setAttribute(Qt::WA_NoSystemBackground);
    auto *recommendationLayout = new QVBoxLayout(recommendationContent);
    recommendationLayout->setContentsMargins(0, 0, 7, 0);
    recommendationLayout->setSpacing(4);
    if (!preferredGameVersion.isEmpty() || !preferredLoader.isEmpty()) {
        auto *loadingRow = new QWidget(recommendationContent);
        auto *loadingRowLayout = new QHBoxLayout(loadingRow);
        loadingRowLayout->setContentsMargins(0, 5, 0, 5);
        loadingRowLayout->setSpacing(7);
        auto *indicator = new LoadingIndicator(loadingRow, Theme::Accent);
        indicator->setFixedSize(17, 17);
        loadingRowLayout->addWidget(indicator);
        loadingRowLayout->addWidget(mutedLabel(
            QStringLiteral("正在整理匹配的 Mod 版本…"), loadingRow));
        loadingRowLayout->addStretch();
        recommendationLayout->addWidget(loadingRow);
    }
    recommendationScroll->setWidget(recommendationContent);
    recommendationCardLayout->addWidget(recommendationScroll);
    root->addWidget(recommendationCard);

    auto *versionTitle = Theme::label(QStringLiteral("按 Minecraft 版本浏览下载"),
                                      15, QFont::DemiBold, Theme::TextPrimary, dialog);
    root->addWidget(versionTitle);
    auto *versionScroll = new QScrollArea(dialog);
    versionScroll->setWidgetResizable(true);
    versionScroll->setFrameShape(QFrame::NoFrame);
    versionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    versionScroll->viewport()->setAutoFillBackground(false);
    auto *versionsContent = new QWidget();
    versionsContent->setAttribute(Qt::WA_NoSystemBackground);
    auto *versionsLayout = new QVBoxLayout(versionsContent);
    versionsLayout->setContentsMargins(0, 0, 7, 0);
    versionsLayout->setSpacing(7);
    versionsLayout->addWidget(mutedLabel(QStringLiteral("正在加载版本列表…"), versionsContent));
    versionScroll->setWidget(versionsContent);
    root->addWidget(versionScroll, 1);

    const auto downloadCallback =
        [this](const QUrl &url, const QString &path, const QByteArray &sha1,
               const QString &modName) {
            return startDownload(url, path, sha1, modName);
        };
    requestProjectVersions(projectId, dialog, recommendationLayout, versionsLayout,
                           m_gameDirectory, m_gameVersionIds,
                           preferredGameVersion, preferredLoader, downloadCallback);
    dialog->show();
}

bool DownloadModsPage::startDownload(const QUrl &url,
                                     const QString &fileName,
                                     const QByteArray &sha1,
                                     const QString &modName)
{
    if (m_downloadTask) {
        emit notifyRequested(QStringLiteral("已有模组正在下载，请等待完成后再试。"));
        return false;
    }
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))) {
        emit notifyRequested(QStringLiteral("模组下载链接无效。"));
        return false;
    }

    m_downloadName->setText(QStringLiteral("正在下载：%1").arg(modName));
    m_downloadSpeed->setText(QStringLiteral("正在连接…"));
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_downloadPanel->show();
    m_previousBytes = 0;
    m_speedClock.restart();

    auto *task = new DownloadTask(url, fileName, sha1, this);
    m_downloadTask = task;
    m_cancelModDownloadButton->setEnabled(true);
    m_cancelModDownloadButton->show();
    disconnect(m_cancelModDownloadButton, nullptr, this, nullptr);
    connect(m_cancelModDownloadButton, &QPushButton::clicked, this, [this, task]() {
        if (m_downloadTask != task || task->isFinished()) {
            return;
        }
        m_cancelModDownloadButton->setEnabled(false);
        m_downloadSpeed->setText(QStringLiteral("正在取消…"));
        task->cancel();
    });
    connect(task, &DownloadTask::bytesProgressChanged, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            const int percent = static_cast<int>(received * 100 / total);
            m_downloadProgress->setRange(0, 100);
            Theme::setProgressValue(m_downloadProgress, qBound(0, percent, 100));
        } else {
            m_downloadProgress->setRange(0, 0);
        }

        const qint64 elapsed = m_speedClock.elapsed();
        if (elapsed >= 500) {
            const qint64 bytesPerSecond =
                qMax<qint64>(0, received - m_previousBytes) * 1000 / elapsed;
            m_previousBytes = received;
            m_speedClock.restart();
            const QString speed = bytesPerSecond >= 1024 * 1024
                ? QStringLiteral("%1 MB/s").arg(bytesPerSecond / 1024.0 / 1024.0, 0, 'f', 2)
                : QStringLiteral("%1 KB/s").arg(bytesPerSecond / 1024.0, 0, 'f', 1);
            const QString totalText = total > 0
                ? QStringLiteral("%1 / %2")
                      .arg(QLocale().formattedDataSize(received),
                           QLocale().formattedDataSize(total))
                : QLocale().formattedDataSize(received);
            m_downloadSpeed->setText(QStringLiteral("%1  ·  %2").arg(speed, totalText));
        }
    });
    connect(task, &DownloadTask::succeeded, this, [this, modName](const QString &path) {
        m_downloadProgress->setRange(0, 100);
        Theme::setProgressValue(m_downloadProgress, 100);
        m_downloadName->setText(QStringLiteral("下载完成：%1").arg(modName));
        m_downloadSpeed->setText(QDir::toNativeSeparators(path));
        emit modsChanged();
        emit notifyRequested(QStringLiteral("%1 下载完成。").arg(modName));
    });
    connect(task, &DownloadTask::failed, this, [this, modName](const QString &reason) {
        if (reason == QStringLiteral("下载已取消。")) {
            m_downloadName->setText(QStringLiteral("已取消：%1").arg(modName));
            m_downloadSpeed->setText(QStringLiteral("下载已中断"));
            m_downloadProgress->setRange(0, 100);
            Theme::setProgressValue(m_downloadProgress, 0);
            emit notifyRequested(QStringLiteral("%1 下载已取消。").arg(modName));
            return;
        }
        m_downloadName->setText(QStringLiteral("下载失败：%1").arg(modName));
        m_downloadSpeed->setText(reason);
        m_downloadProgress->setRange(0, 100);
        emit notifyRequested(QStringLiteral("%1 下载失败：%2").arg(modName, reason));
    });
    connect(task, &DownloadTask::finished, this, [this, task]() {
        if (m_downloadTask == task) {
            m_downloadTask = nullptr;
            m_cancelModDownloadButton->hide();
            m_cancelModDownloadButton->setEnabled(true);
        }
        task->deleteLater();
    });
    task->start();
    return true;
}
