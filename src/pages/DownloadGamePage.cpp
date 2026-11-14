/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DownloadGamePage.h"
#include "GameInstaller.h"
#include "LoaderSetupView.h"
#include "MinecraftPaths.h"
#include "VersionManifest.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "LoadingIndicator.h"
#include "SearchField.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QShowEvent>
#include <QStringList>
#include <QVBoxLayout>

namespace {

class ReleaseTile : public QWidget
{
public:
    ReleaseTile(const QColor &accent, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_accent(accent)
    {
        setFixedSize(46, 46);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF tile = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor background = m_accent;
        background.setAlpha(22);
        QColor border = m_accent;
        border.setAlpha(55);
        painter.setPen(QPen(border, 1.0));
        painter.setBrush(background);
        painter.drawRoundedRect(tile, 13, 13);
        Icons::paint(painter, Icons::Type::Gamepad,
                     tile.adjusted(12, 12, -12, -12), m_accent, 1.6);
    }

private:
    QColor m_accent;
};

QColor typeColor(const QString &category)
{
    if (category == QStringLiteral("快照版")) {
        return Theme::Purple;
    }
    if (category == QStringLiteral("远古版")) {
        return Theme::Amber;
    }
    if (category == QStringLiteral("正式版")) {
        return Theme::Accent;
    }
    return Theme::TextMuted;
}

bool saveVersionDisplayName(const QString &gameDirectory,
                            const QString &versionId,
                            const QString &displayName,
                            QString *errorMessage)
{
    const QString metadataPath = QDir(gameDirectory).filePath(
        QStringLiteral("versions/%1/launcher.json").arg(versionId));
    if (displayName == versionId) {
        if (QFileInfo::exists(metadataPath) && !QFile::remove(metadataPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法清除旧的自定义名称。");
            }
            return false;
        }
        return true;
    }

    QSaveFile metadataFile(metadataPath);
    const QJsonObject metadata{{QStringLiteral("displayName"), displayName}};
    const QByteArray metadataJson =
        QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    if (!metadataFile.open(QIODevice::WriteOnly)
        || metadataFile.write(metadataJson) != metadataJson.size()
        || !metadataFile.commit()) {
        if (errorMessage) {
            *errorMessage = metadataFile.errorString();
        }
        return false;
    }
    return true;
}

bool removeMatchingVersionDisplayName(const QString &gameDirectory,
                                      const QString &versionId,
                                      const QString &displayName,
                                      QString *errorMessage)
{
    const QString metadataPath = QDir(gameDirectory).filePath(
        QStringLiteral("versions/%1/launcher.json").arg(versionId));
    QFile metadataFile(metadataPath);
    if (!metadataFile.exists()) {
        return true;
    }
    if (!metadataFile.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = metadataFile.errorString();
        }
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll());
    if (document.isObject()
        && document.object().value(QStringLiteral("displayName")).toString()
               == displayName) {
        if (!QFile::remove(metadataPath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法移除基础版本的重复自定义名称。");
            }
            return false;
        }
    }
    return true;
}

bool setVersionMetadataFlag(const QString &gameDirectory,
                            const QString &versionId,
                            const QString &key,
                            bool value,
                            QString *errorMessage)
{
    const QString metadataPath = QDir(gameDirectory).filePath(
        QStringLiteral("versions/%1/launcher.json").arg(versionId));
    QJsonObject metadata;
    QFile existingFile(metadataPath);
    if (existingFile.exists()) {
        if (!existingFile.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = existingFile.errorString();
            }
            return false;
        }
        const QJsonDocument existingDocument =
            QJsonDocument::fromJson(existingFile.readAll());
        if (!existingDocument.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("版本元数据格式无效。");
            }
            return false;
        }
        metadata = existingDocument.object();
    }

    if (value) {
        metadata.insert(key, true);
    } else {
        metadata.remove(key);
    }

    QSaveFile output(metadataPath);
    const QByteArray contents =
        QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(contents) != contents.size()
        || !output.commit()) {
        if (errorMessage) {
            *errorMessage = output.errorString();
        }
        return false;
    }
    return true;
}

QString formatReleaseDate(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QStringLiteral("日期未知");
    }
    const QLocale locale(QLocale::Chinese, QLocale::China);
    return locale.toString(dateTime.date(), QStringLiteral("yyyy 年 M 月 d 日"));
}

QString formatSpeed(qint64 bytesPerSecond)
{
    if (bytesPerSecond <= 0) {
        return QStringLiteral("—");
    }
    const double megabytes = static_cast<double>(bytesPerSecond) / (1024.0 * 1024.0);
    if (megabytes >= 1.0) {
        return QStringLiteral("%1 MB/s").arg(megabytes, 0, 'f', 1);
    }
    const double kilobytes = static_cast<double>(bytesPerSecond) / 1024.0;
    return QStringLiteral("%1 KB/s").arg(kilobytes, 0, 'f', 0);
}

} // namespace

DownloadGamePage::DownloadGamePage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_views = new QStackedWidget(this);
    outer->addWidget(m_views);

    m_listPage = new QWidget(m_views);
    m_listPage->setAttribute(Qt::WA_NoSystemBackground);
    m_views->addWidget(m_listPage);
    m_views->setCurrentWidget(m_listPage);

    auto *root = new QVBoxLayout(m_listPage);
    root->setContentsMargins(20, 14, 20, 12);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);
    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(3);
    headerText->addWidget(Theme::label(QStringLiteral("原版游戏"), 23, QFont::DemiBold,
                                       Theme::TextPrimary, m_listPage, true));
    headerText->addWidget(Theme::label(QStringLiteral("选择 Minecraft 版本，准备好下一次启程。"),
                                       12, QFont::Normal, Theme::TextMuted, m_listPage));
    header->addLayout(headerText);
    header->addStretch();
    root->addLayout(header);
    root->addSpacing(10);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(8);
    m_search = new SearchField(QStringLiteral("搜索版本号，例如 1.21.4"), m_listPage);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        m_page = 0;
        refreshCards();
        if (m_versionScroll) {
            m_versionScroll->verticalScrollBar()->setValue(0);
        }
    });
    toolbar->addWidget(m_search);
    toolbar->addStretch();

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    const QStringList filters = {
        QStringLiteral("全部"),
        QStringLiteral("正式版"),
        QStringLiteral("快照版"),
        QStringLiteral("远古版"),
    };
    int chipIndex = 0;
    for (const QString &filter : filters) {
        auto *chip = new AccentButton(filter, m_listPage);
        chip->setVariant(AccentButton::Variant::Ghost);
        chip->setCompact(true);
        chip->setCheckable(true);
        chip->setAutoExclusive(false);
        chip->setAccentColor(Theme::Accent);
        if (filter == m_filter) {
            chip->setChecked(true);
        }
        group->addButton(chip, chipIndex);
        toolbar->addWidget(chip);
        ++chipIndex;
    }
    connect(group, &QButtonGroup::idClicked, this, [this, filters](int id) {
        if (id < 0 || id >= filters.size()) {
            return;
        }
        m_filter = filters.at(id);
        m_page = 0;
        refreshCards();
        if (m_versionScroll) {
            m_versionScroll->verticalScrollBar()->setValue(0);
        }
    });
    root->addLayout(toolbar);
    root->addSpacing(5);

    auto *manifestRow = new QHBoxLayout();
    manifestRow->setContentsMargins(2, 0, 2, 0);
    manifestRow->setSpacing(8);
    m_manifestLoadingIndicator = new LoadingIndicator(m_listPage, Theme::Accent);
    m_manifestLoadingIndicator->setFixedSize(17, 17);
    manifestRow->addWidget(m_manifestLoadingIndicator);
    m_manifestStatusLabel = Theme::label(QStringLiteral("正在获取官方版本清单…"),
                                         10, QFont::Normal, Theme::TextMuted, m_listPage);
    manifestRow->addWidget(m_manifestStatusLabel);
    manifestRow->addStretch();
    m_retryManifestButton = new AccentButton(QStringLiteral("重试"), Icons::Type::Refresh,
                                             m_listPage);
    m_retryManifestButton->setVariant(AccentButton::Variant::Ghost);
    m_retryManifestButton->setCompact(true);
    m_retryManifestButton->hide();
    connect(m_retryManifestButton, &QAbstractButton::clicked,
            this, &DownloadGamePage::fetchManifest);
    manifestRow->addWidget(m_retryManifestButton);
    root->addLayout(manifestRow);
    root->addSpacing(6);

    m_progressPanel = new CardWidget(m_listPage);
    m_progressPanel->setAccentColor(Theme::Accent);
    m_progressPanel->setCornerRadius(14);
    m_progressPanel->setFixedHeight(88);
    auto *progressLayout = new QVBoxLayout(m_progressPanel);
    progressLayout->setContentsMargins(18, 12, 18, 12);
    progressLayout->setSpacing(0);

    auto *progressTop = new QHBoxLayout();
    progressTop->setContentsMargins(0, 0, 0, 0);
    progressTop->setSpacing(10);
    m_progressTitle = Theme::label(QStringLiteral("准备下载"), 12, QFont::DemiBold,
                                   Theme::TextPrimary, m_progressPanel);
    m_progressPercent = Theme::label(QStringLiteral("0%"), 11, QFont::DemiBold,
                                     Theme::Accent, m_progressPanel);
    m_progressPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_progressSpeed = Theme::label(QStringLiteral("—"), 10, QFont::Normal,
                                   Theme::TextMuted, m_progressPanel);
    m_progressSpeed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressTop->addWidget(m_progressTitle, 1);
    progressTop->addWidget(m_progressSpeed);
    progressTop->addSpacing(10);
    progressTop->addWidget(m_progressPercent);
    m_cancelDownloadButton = new AccentButton(QStringLiteral("取消"), m_progressPanel);
    m_cancelDownloadButton->setVariant(AccentButton::Variant::Ghost);
    m_cancelDownloadButton->setCompact(true);
    m_cancelDownloadButton->hide();
    connect(m_cancelDownloadButton, &QAbstractButton::clicked, this, [this]() {
        if (!m_installing) {
            return;
        }
        m_cancelDownloadButton->setEnabled(false);
        m_progressDetail->setText(QStringLiteral("正在取消…"));
        if (m_loaderDownloadActive && m_loaderDownloader) {
            m_loaderDownloader->cancel();
        } else {
            m_installer->cancel();
        }
    });
    progressTop->addWidget(m_cancelDownloadButton);
    progressLayout->addLayout(progressTop);
    progressLayout->addSpacing(7);

    m_progressBar = new QProgressBar(m_progressPanel);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    progressLayout->addWidget(m_progressBar);
    progressLayout->addSpacing(5);

    m_progressDetail = Theme::label(QString(), 10, QFont::Normal,
                                    Theme::TextMuted, m_progressPanel);
    progressLayout->addWidget(m_progressDetail);
    m_progressPanel->hide();
    root->addWidget(m_progressPanel);
    root->addSpacing(10);

    const QString defaultGameDir = MinecraftPaths::savedGameDirectory();
    m_installer = new GameInstaller(defaultGameDir, this);
    m_progressPanel->setToolTip(QDir::toNativeSeparators(defaultGameDir));

    connect(m_installer, &GameInstaller::phaseChanged, this,
            [this](const QString &phase) {
                m_progressDetail->setText(phase);
            });
    connect(m_installer, &GameInstaller::overallPercentChanged, this,
            [this](int percent) {
                Theme::setProgressValue(m_progressBar, percent);
                m_progressPercent->setText(QStringLiteral("%1%").arg(percent));
            });
    connect(m_installer, &GameInstaller::speedChanged, this,
            [this](qint64 bytesPerSecond) {
                m_progressSpeed->setText(formatSpeed(bytesPerSecond));
            });
    connect(m_installer, &GameInstaller::succeeded, this,
            [this](const QString &versionId) {
                if (m_loaderSelection.hasLoader()) {
                    QString metadataError;
                    if (!removeMatchingVersionDisplayName(
                            m_installer->gameDirectory(), versionId,
                            m_customVersionName, &metadataError)) {
                        emit notifyRequested(
                            QStringLiteral("基础版本已下载，但清理重复名称失败：%1")
                                .arg(metadataError));
                    }
                    metadataError.clear();
                    if (!setVersionMetadataFlag(m_installer->gameDirectory(), versionId,
                                                QStringLiteral("loaderBaseOnly"), true,
                                                &metadataError)) {
                        emit notifyRequested(
                            QStringLiteral("加载器基础版本已下载，但无法记录基础版本标记：%1")
                                .arg(metadataError));
                    }
                } else {
                    QString metadataError;
                    if (!saveVersionDisplayName(m_installer->gameDirectory(), versionId,
                                                m_customVersionName, &metadataError)) {
                        emit notifyRequested(
                            QStringLiteral("版本已下载，但无法保存自定义名称：%1")
                                .arg(metadataError));
                    }
                    metadataError.clear();
                    if (!setVersionMetadataFlag(m_installer->gameDirectory(), versionId,
                                                QStringLiteral("loaderBaseOnly"), false,
                                                &metadataError)) {
                        emit notifyRequested(
                            QStringLiteral("版本已下载，但无法清除旧的基础版本标记：%1")
                                .arg(metadataError));
                    }
                }
                if (m_loaderSelection.hasLoader()) {
                    m_progressTitle->setText(
                        QStringLiteral("Minecraft %1 基础文件下载完成").arg(versionId));
                    m_progressDetail->setText(
                        QStringLiteral("正在准备下载 %1…").arg(m_loaderSelection.loader));
                    startLoaderDownload();
                    return;
                }
                finalizeDownload(
                    QStringLiteral("Minecraft %1 基础文件下载完成。").arg(versionId));
            });
    connect(m_installer, &GameInstaller::failed, this,
            [this](const QString &reason) {
                m_installing = false;
                m_loaderDownloadActive = false;
                m_loaderSelection = LoaderSelection();
                m_cancelDownloadButton->hide();
                updateInstallButtonsEnabled();
                m_progressTitle->setText(QStringLiteral("下载失败"));
                m_progressDetail->setText(reason);
                m_progressPercent->setText(QStringLiteral("—"));
                m_progressSpeed->setText(QStringLiteral("—"));
                Theme::setProgressValue(m_progressBar, 0);
                emit notifyRequested(QStringLiteral("下载失败：%1").arg(reason));
            });
    connect(m_installer, &GameInstaller::cancelled, this, [this]() {
        m_installing = false;
        m_loaderDownloadActive = false;
        m_loaderSelection = LoaderSelection();
        m_cancelDownloadButton->hide();
        m_cancelDownloadButton->setEnabled(true);
        updateInstallButtonsEnabled();
        m_progressTitle->setText(QStringLiteral("下载已取消"));
        m_progressDetail->setText(QStringLiteral("已取消未完成的下载文件。"));
        m_progressPercent->setText(QStringLiteral("—"));
        m_progressSpeed->setText(QStringLiteral("—"));
        Theme::setProgressValue(m_progressBar, 0);
        emit notifyRequested(QStringLiteral("游戏版本下载已取消。"));
    });

    auto *scroll = new QScrollArea(m_listPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget();
    content->setAttribute(Qt::WA_NoSystemBackground);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 7, 4);
    contentLayout->setSpacing(0);

    auto *featured = new CardWidget(content);
    featured->setAccentColor(Theme::Accent);
    featured->setCornerRadius(20.0);
    featured->setFixedHeight(140);
    auto *featuredLayout = new QHBoxLayout(featured);
    featuredLayout->setContentsMargins(20, 14, 20, 14);
    featuredLayout->setSpacing(12);

    auto *featuredText = new QVBoxLayout();
    featuredText->setContentsMargins(0, 0, 0, 0);
    featuredText->setSpacing(3);
    QColor latestBackground = Theme::Accent;
    latestBackground.setAlpha(24);
    auto *latestBadge = Theme::badge(QStringLiteral("最新正式版"), Theme::Accent,
                                     latestBackground, featured,
                                     QColor(Theme::Accent.red(), Theme::Accent.green(),
                                            Theme::Accent.blue(), 65));
    featuredText->addWidget(latestBadge, 0, Qt::AlignLeft);
    featuredText->addSpacing(2);
    m_featuredVersionLabel = Theme::label(QStringLiteral("正在获取最新正式版…"),
                                          24, QFont::DemiBold, Theme::TextPrimary,
                                          featured, true);
    featuredText->addWidget(m_featuredVersionLabel);
    m_featuredMetaLabel = Theme::label(QStringLiteral("请稍候"),
                                       11, QFont::Normal, Theme::TextMuted, featured);
    featuredText->addWidget(m_featuredMetaLabel);
    featuredText->addSpacing(4);

    auto *featuredActions = new QHBoxLayout();
    featuredActions->setContentsMargins(0, 0, 0, 0);
    featuredActions->setSpacing(12);
    auto *install = new AccentButton(QStringLiteral("安装此版本"),
                                     Icons::Type::Download, featured);
    install->setVariant(AccentButton::Variant::Primary);
    install->setMinimumWidth(132);
    install->setEnabled(false);
    m_featuredInstallButton = install;
    connect(install, &QAbstractButton::clicked, this, [this]() {
        if (!m_featuredVersionId.isEmpty()) {
            startInstall(m_featuredVersionId);
        }
    });
    featuredActions->addWidget(install);
    featuredActions->addWidget(Theme::label(QStringLiteral("客户端 · 依赖库 · 资源索引"),
                                            10, QFont::Normal, Theme::TextMuted, featured));
    featuredActions->addStretch();
    featuredText->addLayout(featuredActions);
    featuredLayout->addLayout(featuredText, 1);
    contentLayout->addWidget(featured);
    contentLayout->addSpacing(12);

    auto *listHeader = new QHBoxLayout();
    listHeader->setContentsMargins(2, 0, 2, 0);
    listHeader->setSpacing(10);
    listHeader->addWidget(Theme::label(QStringLiteral("所有版本"), 16, QFont::DemiBold,
                                       Theme::TextPrimary, content));
    m_countLabel = Theme::label(QString(), 10, QFont::Normal, Theme::TextMuted, content);
    listHeader->addWidget(m_countLabel);
    listHeader->addStretch();
    listHeader->addWidget(Theme::label(QStringLiteral("安装时读取官方版本清单"), 10,
                                       QFont::Normal, Theme::TextMuted, content));
    contentLayout->addLayout(listHeader);
    contentLayout->addSpacing(7);

    m_grid = new QGridLayout();
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(8);
    contentLayout->addLayout(m_grid);

    m_emptyLabel = Theme::label(QStringLiteral("没有找到匹配的版本，换个关键词试试。"), 12,
                                QFont::Normal, Theme::TextMuted, content);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFixedHeight(100);
    m_emptyLabel->hide();
    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addSpacing(10);

    auto *pager = new QHBoxLayout();
    pager->setContentsMargins(2, 0, 2, 0);
    pager->setSpacing(8);
    pager->addStretch();

    m_prevPageButton = new AccentButton(QStringLiteral("上一页"), content);
    m_prevPageButton->setVariant(AccentButton::Variant::Ghost);
    m_prevPageButton->setCompact(true);
    m_prevPageButton->hide();
    pager->addWidget(m_prevPageButton);

    m_pageLabel = Theme::label(QString(), 10, QFont::Normal, Theme::TextMuted, content);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    pager->addWidget(m_pageLabel);

    m_nextPageButton = new AccentButton(QStringLiteral("下一页"), content);
    m_nextPageButton->setVariant(AccentButton::Variant::Ghost);
    m_nextPageButton->setCompact(true);
    m_nextPageButton->hide();
    pager->addWidget(m_nextPageButton);
    pager->addStretch();
    contentLayout->addLayout(pager);
    contentLayout->addStretch(1);

    connect(m_prevPageButton, &QAbstractButton::clicked, this, [this]() {
        if (m_page > 0) {
            --m_page;
            refreshCards();
            if (m_versionScroll) {
                m_versionScroll->verticalScrollBar()->setValue(0);
            }
        }
    });
    connect(m_nextPageButton, &QAbstractButton::clicked, this, [this]() {
        ++m_page;
        refreshCards();
        if (m_versionScroll) {
            m_versionScroll->verticalScrollBar()->setValue(0);
        }
    });

    m_versionScroll = scroll;
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    m_manifestClient = new VersionManifestClient(this);
    connect(m_manifestClient, &VersionManifestClient::ready,
            this, &DownloadGamePage::handleManifestReady);
    connect(m_manifestClient, &VersionManifestClient::failed,
            this, &DownloadGamePage::handleManifestFailed);

}

void DownloadGamePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_pendingManifestError.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("版本清单加载失败"),
                             m_pendingManifestError);
        m_pendingManifestError.clear();
    }
    if (!m_initialManifestStarted) {
        m_initialManifestStarted = true;
        fetchManifest();
    }
}

void DownloadGamePage::fetchManifest()
{
    if (!m_manifestClient) {
        return;
    }

    m_manifestLoading = true;
    m_versions.clear();
    m_latestReleaseId.clear();
    m_latestSnapshotId.clear();
    m_featuredVersionId.clear();
    m_featuredVersionUrl.clear();
    m_featuredVersionSha1.clear();
    m_page = 0;

    m_featuredVersionLabel->setText(QStringLiteral("正在获取最新正式版…"));
    m_featuredMetaLabel->setText(QStringLiteral("请稍候"));
    m_featuredInstallButton->setEnabled(false);

    m_manifestStatusLabel->setText(QStringLiteral("正在获取官方版本清单…"));
    m_manifestStatusLabel->show();
    m_manifestLoadingIndicator->show();
    m_retryManifestButton->hide();

    refreshCards();
    m_manifestClient->fetch();
}

void DownloadGamePage::handleManifestReady(const MinecraftVersionManifest &manifest)
{
    m_manifestLoading = false;
    m_manifestLoadingIndicator->hide();
    m_versions = manifest.versions;
    m_latestReleaseId = manifest.latestRelease;
    m_latestSnapshotId = manifest.latestSnapshot;
    m_page = 0;

    m_manifestStatusLabel->hide();
    m_retryManifestButton->hide();

    updateFeatured();
    refreshCards();
}

void DownloadGamePage::handleManifestFailed(const QString &reason)
{
    m_manifestLoading = false;
    m_manifestLoadingIndicator->hide();
    m_versions.clear();
    m_page = 0;

    m_featuredVersionId.clear();
    m_featuredVersionUrl.clear();
    m_featuredVersionSha1.clear();
    m_featuredVersionLabel->setText(QStringLiteral("版本清单暂不可用"));
    m_featuredMetaLabel->setText(QStringLiteral("请检查网络后重试"));
    m_featuredInstallButton->setEnabled(false);

    m_manifestStatusLabel->setText(reason);
    m_manifestStatusLabel->show();
    m_retryManifestButton->show();

    refreshCards();
    m_pendingManifestError =
        QStringLiteral("%1\n\n请检查网络连接后重试。").arg(reason);
    if (isVisible()) {
        QMessageBox::warning(this, QStringLiteral("版本清单加载失败"),
                             m_pendingManifestError);
        m_pendingManifestError.clear();
    }
}

void DownloadGamePage::updateFeatured()
{
    const MinecraftVersionEntry *featured = nullptr;
    for (const MinecraftVersionEntry &entry : m_versions) {
        if (entry.id == m_latestReleaseId) {
            featured = &entry;
            break;
        }
    }
    if (!featured) {
        for (const MinecraftVersionEntry &entry : m_versions) {
            if (entry.type == QStringLiteral("release")) {
                featured = &entry;
                break;
            }
        }
    }

    if (!featured) {
        m_featuredVersionId.clear();
        m_featuredVersionUrl.clear();
        m_featuredVersionSha1.clear();
        m_featuredVersionLabel->setText(QStringLiteral("暂无正式版"));
        m_featuredMetaLabel->setText(QStringLiteral("请稍后重试"));
        m_featuredInstallButton->setEnabled(false);
        return;
    }

    m_featuredVersionId = featured->id;
    m_featuredVersionUrl = featured->jsonUrl;
    m_featuredVersionSha1 = featured->jsonSha1;
    m_featuredVersionLabel->setText(QStringLiteral("Minecraft %1").arg(featured->id));
    m_featuredMetaLabel->setText(
        QStringLiteral("官方正式版  ·  发布于 %1")
            .arg(formatReleaseDate(featured->releaseTime)));
    m_featuredInstallButton->setEnabled(!m_installing);
}

void DownloadGamePage::updatePageControls(int totalPages)
{
    const bool visible = totalPages > 1;
    m_prevPageButton->setVisible(visible);
    m_nextPageButton->setVisible(visible);
    m_pageLabel->setVisible(visible);
    if (!visible) {
        m_pageLabel->clear();
        return;
    }

    m_pageLabel->setText(QStringLiteral("第 %1 / %2 页").arg(m_page + 1).arg(totalPages));
    m_prevPageButton->setEnabled(m_page > 0);
    m_nextPageButton->setEnabled(m_page + 1 < totalPages);
}

void DownloadGamePage::setGameDirectory(const QString &directory)
{
    if (!m_installer || m_installing || directory.trimmed().isEmpty()) {
        return;
    }
    m_installer->setGameDirectory(directory);
    if (m_loaderView) {
        m_loaderView->setGameDirectory(directory);
    }
    if (m_progressPanel) {
        m_progressPanel->setToolTip(
            QDir::toNativeSeparators(m_installer->gameDirectory()));
    }
}

void DownloadGamePage::refreshCards()
{
    if (!m_grid) {
        return;
    }

    m_installButtons.clear();
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    if (m_manifestLoading) {
        m_countLabel->setText(QStringLiteral("正在加载版本清单…"));
        m_emptyLabel->hide();
        updatePageControls(1);
        updateInstallButtonsEnabled();
        return;
    }

    if (m_versions.isEmpty()) {
        m_countLabel->setText(QStringLiteral("共 0 个版本"));
        m_emptyLabel->setText(QStringLiteral("暂无可用的游戏版本。"));
        m_emptyLabel->show();
        updatePageControls(1);
        updateInstallButtonsEnabled();
        return;
    }

    const QString query = m_search ? m_search->text().trimmed() : QString();
    QVector<const MinecraftVersionEntry *> filtered;
    filtered.reserve(m_versions.size());
    for (const MinecraftVersionEntry &entry : m_versions) {
        const QString category = versionCategory(entry.type);
        if (m_filter != QStringLiteral("全部") && category != m_filter) {
            continue;
        }
        if (!query.isEmpty()
            && !entry.id.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(&entry);
    }

    const int total = filtered.size();
    const int totalPages = qMax(1, (total + kPageSize - 1) / kPageSize);
    m_page = qBound(0, m_page, totalPages - 1);

    if (total == 0) {
        m_countLabel->setText(QStringLiteral("共 0 个版本"));
        m_emptyLabel->setText(
            QStringLiteral("没有找到匹配的版本，换个关键词或分类试试。"));
        m_emptyLabel->show();
        updatePageControls(1);
        updateInstallButtonsEnabled();
        return;
    }

    const int start = m_page * kPageSize;
    const int end = qMin(start + kPageSize, total);
    QWidget *cardsParent = m_grid->parentWidget();
    for (int i = start; i < end; ++i) {
        const MinecraftVersionEntry &entry = *filtered.at(i);
        const QString category = versionCategory(entry.type);
        const QColor accent = typeColor(category);

        auto *card = new CardWidget(cardsParent);
        card->setAccentColor(accent);
        card->setCornerRadius(15.0);
        card->setFixedHeight(82);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(15, 11, 15, 11);
        row->setSpacing(13);

        row->addWidget(new ReleaseTile(accent, card), 0, Qt::AlignVCenter);

        auto *text = new QVBoxLayout();
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(0);
        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(7);
        titleRow->addWidget(Theme::label(QStringLiteral("Minecraft %1").arg(entry.id),
                                         14, QFont::DemiBold, Theme::TextPrimary, card));
        QColor typeBackground = accent;
        typeBackground.setAlpha(20);
        QColor typeBorder = accent;
        typeBorder.setAlpha(52);
        titleRow->addWidget(Theme::badge(category, accent, typeBackground, card, typeBorder));
        titleRow->addStretch();
        text->addLayout(titleRow);

        QString meta = formatReleaseDate(entry.releaseTime);
        if (entry.id == m_latestReleaseId) {
            meta.prepend(QStringLiteral("最新正式版  ·  "));
        } else if (entry.id == m_latestSnapshotId) {
            meta.prepend(QStringLiteral("最新快照  ·  "));
        }
        text->addWidget(Theme::label(meta, 10, QFont::Normal, Theme::TextMuted, card));
        row->addLayout(text, 1);

        auto *install = new AccentButton(QStringLiteral("安装"), Icons::Type::Download, card);
        install->setVariant(AccentButton::Variant::Secondary);
        install->setCompact(true);
        install->setAccentColor(accent);
        install->setAccessibleName(QStringLiteral("安装 Minecraft %1").arg(entry.id));
        connect(install, &QAbstractButton::clicked, this, [this, version = entry.id]() {
            startInstall(version);
        });
        m_installButtons.append(install);
        row->addWidget(install, 0, Qt::AlignVCenter);

        m_grid->addWidget(card, (i - start) / 2, (i - start) % 2);
    }

    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    m_countLabel->setText(QStringLiteral("共 %1 个版本").arg(total));
    m_emptyLabel->hide();
    updatePageControls(totalPages);
    updateInstallButtonsEnabled();
}

void DownloadGamePage::openLoaderSetup(const QString &version)
{
    if (m_installing || m_manifestLoading) {
        return;
    }

    const MinecraftVersionEntry *entry = nullptr;
    for (const MinecraftVersionEntry &candidate : m_versions) {
        if (candidate.id == version) {
            entry = &candidate;
            break;
        }
    }
    if (!entry) {
        emit notifyRequested(QStringLiteral("找不到版本 %1 的下载信息。").arg(version));
        return;
    }

    m_baseVersion = entry->id;
    m_baseJsonUrl = entry->jsonUrl;
    m_baseSha1 = entry->jsonSha1;
    m_loaderSelection = LoaderSelection();

    if (m_loaderView) {
        m_loaderView->deleteLater();
        m_loaderView = nullptr;
    }

    m_loaderView = new LoaderSetupView(version, m_installer->gameDirectory(), m_views);
    m_views->addWidget(m_loaderView);
    connect(m_loaderView, &LoaderSetupView::backRequested, this, [this]() {
        m_views->setCurrentWidget(m_listPage);
    });
    connect(m_loaderView, &LoaderSetupView::notifyRequested,
            this, &DownloadGamePage::notifyRequested);
    connect(m_loaderView, &LoaderSetupView::startRequested,
            this, &DownloadGamePage::beginSelectedDownload);
    m_views->setCurrentWidget(m_loaderView);
}

void DownloadGamePage::beginSelectedDownload()
{
    if (m_installing || !m_loaderView || m_baseVersion.isEmpty()) {
        return;
    }

    m_loaderSelection = m_loaderView->selection();
    m_customVersionName = m_loaderView->versionName();
    if (m_customVersionName.trimmed().isEmpty()) {
        m_customVersionName = m_baseVersion;
    }
    m_views->setCurrentWidget(m_listPage);

    m_installing = true;
    m_loaderDownloadActive = false;
    updateInstallButtonsEnabled();
    m_progressPanel->show();
    m_cancelDownloadButton->setEnabled(true);
    m_cancelDownloadButton->show();
    m_progressTitle->setText(
        QStringLiteral("正在下载 Minecraft %1").arg(m_baseVersion));
    m_progressPercent->setText(QStringLiteral("0%"));
    m_progressSpeed->setText(QStringLiteral("—"));
    m_progressDetail->setText(
        QStringLiteral("目标目录：%1")
            .arg(QDir::toNativeSeparators(m_installer->gameDirectory())));
    m_progressBar->setRange(0, 100);
    Theme::setProgressValue(m_progressBar, 0);

    m_installer->installFromManifest(m_baseVersion, m_baseJsonUrl, m_baseSha1);
}

void DownloadGamePage::startLoaderDownload()
{
    if (!m_loaderSelection.hasLoader()) {
        finalizeDownload(
            QStringLiteral("Minecraft %1 基础文件下载完成。").arg(m_baseVersion));
        return;
    }

    if (m_loaderDownloader) {
        m_loaderDownloader->deleteLater();
        m_loaderDownloader = nullptr;
    }

    m_loaderDownloadActive = true;
    m_loaderDownloader =
        new LoaderDownloader(m_installer->gameDirectory(), this);

    connect(m_loaderDownloader, &LoaderDownloader::phaseChanged, this,
            [this](const QString &phase) {
                m_progressDetail->setText(phase);
                if (phase.contains(QStringLiteral("自动安装"))) {
                    m_progressTitle->setText(
                        QStringLiteral("正在安装 %1").arg(m_loaderSelection.loader));
                }
            });
    connect(m_loaderDownloader, &LoaderDownloader::percentChanged, this,
            [this](int percent) {
                Theme::setProgressValue(m_progressBar, percent);
                m_progressPercent->setText(QStringLiteral("%1%").arg(percent));
            });
    connect(m_loaderDownloader, &LoaderDownloader::speedChanged, this,
            [this](qint64 bytesPerSecond) {
                m_progressSpeed->setText(formatSpeed(bytesPerSecond));
            });
    connect(m_loaderDownloader, &LoaderDownloader::finished, this,
            [this](const QString &summary) {
                m_loaderDownloadActive = false;
                QString versionId = m_loaderDownloader
                    ? m_loaderDownloader->installedVersionId()
                    : QString();
                if (versionId.isEmpty()) {
                    versionId = m_baseVersion;
                }
                QString metadataError;
                if (!saveVersionDisplayName(m_installer->gameDirectory(), versionId,
                                            m_customVersionName, &metadataError)) {
                    emit notifyRequested(
                        QStringLiteral("加载器已安装，但无法保存自定义名称：%1")
                            .arg(metadataError));
                }
                finalizeDownload(summary);
            });
    connect(m_loaderDownloader, &LoaderDownloader::failed, this,
            [this](const QString &reason) {
                m_loaderDownloadActive = false;
                QString metadataError;
                if (!saveVersionDisplayName(m_installer->gameDirectory(), m_baseVersion,
                                            m_customVersionName, &metadataError)) {
                    emit notifyRequested(
                        QStringLiteral("基础游戏已下载，但无法保存自定义名称：%1")
                            .arg(metadataError));
                }
                handleLoaderFailed(reason);
            });
    connect(m_loaderDownloader, &LoaderDownloader::cancelled, this, [this]() {
        QString metadataError;
        if (!saveVersionDisplayName(m_installer->gameDirectory(), m_baseVersion,
                                    m_customVersionName, &metadataError)) {
            emit notifyRequested(
                QStringLiteral("基础游戏已下载，但无法保存自定义名称：%1")
                    .arg(metadataError));
        }
        m_installing = false;
        m_loaderDownloadActive = false;
        m_loaderSelection = LoaderSelection();
        m_cancelDownloadButton->hide();
        m_cancelDownloadButton->setEnabled(true);
        updateInstallButtonsEnabled();
        m_progressTitle->setText(QStringLiteral("下载已取消"));
        m_progressDetail->setText(QStringLiteral("已取消加载器下载或安装。"));
        m_progressPercent->setText(QStringLiteral("—"));
        m_progressSpeed->setText(QStringLiteral("—"));
        Theme::setProgressValue(m_progressBar, 0);
        emit notifyRequested(QStringLiteral("加载器下载已取消。"));
    });

    m_progressTitle->setText(
        QStringLiteral("正在下载 %1 %2")
            .arg(m_loaderSelection.loader, m_loaderSelection.loaderVersion));
    m_progressPercent->setText(QStringLiteral("0%"));
    m_progressSpeed->setText(QStringLiteral("—"));
    Theme::setProgressValue(m_progressBar, 0);
    m_progressDetail->setText(QStringLiteral("正在准备下载组件…"));
    m_loaderDownloader->start(m_baseVersion, m_loaderSelection);
}

void DownloadGamePage::finalizeDownload(const QString &summary)
{
    m_installing = false;
    m_loaderDownloadActive = false;
    m_loaderSelection = LoaderSelection();
    m_cancelDownloadButton->hide();
    updateInstallButtonsEnabled();

    m_progressTitle->setText(QStringLiteral("下载完成"));
    m_progressDetail->setText(summary);
    m_progressPercent->setText(QStringLiteral("100%"));
    m_progressSpeed->setText(QStringLiteral("—"));
    Theme::setProgressValue(m_progressBar, 100);
    emit installSucceeded(m_baseVersion);
    emit notifyRequested(summary);
}

void DownloadGamePage::handleLoaderFailed(const QString &reason)
{
    m_installing = false;
    m_loaderDownloadActive = false;
    m_loaderSelection = LoaderSelection();
    m_cancelDownloadButton->hide();
    updateInstallButtonsEnabled();

    m_progressTitle->setText(QStringLiteral("加载器下载失败"));
    m_progressDetail->setText(reason);
    m_progressPercent->setText(QStringLiteral("—"));
    m_progressSpeed->setText(QStringLiteral("—"));
    Theme::setProgressValue(m_progressBar, 0);
    emit baseInstallFinished(m_baseVersion);
    emit notifyRequested(QStringLiteral("加载器下载失败：%1").arg(reason));
}

void DownloadGamePage::startInstall(const QString &version)
{
    if (m_installing || m_manifestLoading) {
        return;
    }

    m_customVersionName = version;
    openLoaderSetup(version);
}

void DownloadGamePage::updateInstallButtonsEnabled()
{
    const bool enabled = !m_installing && !m_manifestLoading;
    if (m_featuredInstallButton) {
        m_featuredInstallButton->setEnabled(enabled && !m_featuredVersionId.isEmpty());
    }
    for (AccentButton *button : m_installButtons) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}
