/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "HomePage.h"

#include "AccentButton.h"
#include "AnimatedStackedWidget.h"
#include "AnimePortraitWidget.h"
#include "CardWidget.h"
#include "HomeNewsWidget.h"
#include "HomeStatusWidget.h"
#include "IconFactory.h"
#include "LocalVersionScanner.h"
#include "MinecraftPaths.h"
#include "Theme.h"
#include "TurtleAnimationWidget.h"
#include "VersionSelectPage.h"
#include "account/AccountDialog.h"
#include "settings/GlobalSettings.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QSaveFile>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace {

QString portraitResourceForStyle(const QString &style)
{
    static const QHash<QString, QString> resources{
        {QStringLiteral("live2d-maid"), QStringLiteral("portrait-maid.png")},
        {QStringLiteral("live2d-white-hoodie"), QStringLiteral("portrait-white-hoodie.png")},
        {QStringLiteral("live2d-red"), QStringLiteral("portrait-red.png")},
        {QStringLiteral("live2d-pink"), QStringLiteral("portrait-pink.png")},
        {QStringLiteral("live2d-blue-hat"), QStringLiteral("portrait-blue-hat.png")},
        {QStringLiteral("live2d-pink-teddy"), QStringLiteral("portrait-pink-teddy.png")},
        {QStringLiteral("live2d-black-ponytail"), QStringLiteral("portrait-black-ponytail.png")},
        {QStringLiteral("live2d-twintails"), QStringLiteral("portrait-twintails.png")},
        {QStringLiteral("live2d-black-dress"), QStringLiteral("portrait-black-dress.png")},
        {QStringLiteral("live2d-white-hoodie-2"), QStringLiteral("portrait-white-hoodie-2.png")},
        {QStringLiteral("live2d-pink-tee"), QStringLiteral("portrait-pink-tee.png")},
    };
    return resources.value(style, QStringLiteral("portrait-maid.png"));
}

bool markLoaderBaseVersion(const QString &gameDirectory,
                           const QString &versionDirectory,
                           QString *errorMessage)
{
    QString versionJsonPath = QDir(versionDirectory).filePath(
        QFileInfo(versionDirectory).fileName() + QStringLiteral(".json"));
    if (!QFileInfo::exists(versionJsonPath)) {
        const QFileInfoList jsonFiles = QDir(versionDirectory).entryInfoList(
            QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        if (jsonFiles.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("找不到加载器版本 JSON。");
            }
            return false;
        }
        versionJsonPath = jsonFiles.first().absoluteFilePath();
    }
    QFile versionJson(versionJsonPath);
    if (!versionJson.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取加载器版本配置：%1")
                                .arg(versionJson.errorString());
        }
        return false;
    }
    const QJsonDocument versionDocument =
        QJsonDocument::fromJson(versionJson.readAll());
    if (!versionDocument.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("加载器版本 JSON 格式无效。");
        }
        return false;
    }
    const QString baseVersion =
        versionDocument.object().value(QStringLiteral("inheritsFrom"))
            .toString().trimmed();
    if (baseVersion.isEmpty()) {
        return true;
    }
    if (baseVersion == QStringLiteral(".") || baseVersion == QStringLiteral("..")
        || baseVersion.contains(QLatin1Char('/'))
        || baseVersion.contains(QLatin1Char('\\'))
        || baseVersion.contains(QLatin1Char(':'))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("加载器配置中的基础版本 ID 无效。");
        }
        return false;
    }

    const QString metadataPath = QDir(gameDirectory).filePath(
        QStringLiteral("versions/%1/launcher.json").arg(baseVersion));
    QJsonObject metadata;
    QFile metadataFile(metadataPath);
    if (metadataFile.exists()) {
        if (!metadataFile.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法读取基础版本元数据：%1")
                                    .arg(metadataFile.errorString());
            }
            return false;
        }
        const QJsonDocument metadataDocument =
            QJsonDocument::fromJson(metadataFile.readAll());
        if (!metadataDocument.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("基础版本元数据格式无效。");
            }
            return false;
        }
        metadata = metadataDocument.object();
    }
    metadata.insert(QStringLiteral("loaderBaseOnly"), true);

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

QString displayVersionName(const VersionInfo &version)
{
    if (!version.displayName.trimmed().isEmpty()) {
        return version.displayName.trimmed();
    }
    if (!version.versionId.trimmed().isEmpty()) {
        return version.versionId.trimmed();
    }
    return version.minecraft;
}

QString truncatedName(const QString &name, int limit = 28)
{
    return name.size() > limit ? name.left(limit - 1) + QStringLiteral("…") : name;
}

} // namespace

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    const QString configuredDirectory = MinecraftPaths::savedGameDirectory();
    m_gameDirectory = QDir(configuredDirectory.trimmed().isEmpty()
                               ? MinecraftPaths::defaultGameDirectory()
                               : configuredDirectory)
                          .absolutePath();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new AnimatedStackedWidget(this);
    root->addWidget(m_stack);

    m_overviewPage = new QWidget(m_stack);
    m_overviewPage->setAttribute(Qt::WA_NoSystemBackground);
    auto *overviewLayout = new QHBoxLayout(m_overviewPage);
    overviewLayout->setContentsMargins(34, 26, 34, 28);
    overviewLayout->setSpacing(18);

    auto *launchCard = new CardWidget(m_overviewPage);
    launchCard->setAccentColor(Theme::Accent);
    launchCard->setInteractive(true);
    launchCard->setCornerRadius(20);
    launchCard->setMinimumWidth(320);
    launchCard->setMaximumWidth(400);
    launchCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *launchLayout = new QVBoxLayout(launchCard);
    launchLayout->setContentsMargins(24, 22, 24, 22);
    launchLayout->setSpacing(0);

    auto *eyebrow = Theme::label(QStringLiteral("当前版本  /  CURRENT VERSION"),
                                 9, QFont::Bold, Theme::Accent, launchCard);
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    eyebrow->setFont(eyebrowFont);
    launchLayout->addWidget(eyebrow);
    launchLayout->addSpacing(8);

    m_versionNameLabel = Theme::label(QStringLiteral("未选择版本"),
                                      20, QFont::DemiBold,
                                      Theme::TextPrimary, launchCard, true);
    m_versionNameLabel->setWordWrap(true);
    launchLayout->addWidget(m_versionNameLabel);
    launchLayout->addSpacing(4);

    m_versionMetaLabel = Theme::label(QStringLiteral("请先选择一个本地游戏版本。"),
                                      10, QFont::Normal,
                                      Theme::TextMuted, launchCard);
    m_versionMetaLabel->setWordWrap(true);
    launchLayout->addWidget(m_versionMetaLabel);
    launchLayout->addSpacing(14);
    launchLayout->addWidget(Theme::divider(launchCard));
    launchLayout->addSpacing(12);

    m_versionStatsLabel = Theme::label(QString(), 10, QFont::Normal,
                                       Theme::TextSecondary, launchCard);
    m_versionStatsLabel->setWordWrap(true);
    launchLayout->addWidget(m_versionStatsLabel);
    launchLayout->addStretch(1);

    m_scanStatusLabel = Theme::label(QStringLiteral("正在扫描本地版本…"),
                                     10, QFont::Normal, Theme::TextMuted, launchCard);
    m_scanStatusLabel->setWordWrap(true);
    launchLayout->addWidget(m_scanStatusLabel);
    launchLayout->addSpacing(10);

    m_launchButton = new AccentButton(QStringLiteral("启动游戏"), Icons::Type::Play,
                                      launchCard);
    m_launchButton->setVariant(AccentButton::Variant::Primary);
    m_launchButton->setFont(Theme::font(14, QFont::DemiBold));
    m_launchButton->setFixedHeight(52);
    m_launchButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_launchButton, &QAbstractButton::clicked, this, [this]() {
        if (m_selectedVersionIndex < 0
            || m_selectedVersionIndex >= m_versions.size()) {
            emit notifyRequested(QStringLiteral("请先选择一个本地游戏版本。"));
            return;
        }
        emit launchRequested();
    });
    launchLayout->addWidget(m_launchButton);
    launchLayout->addSpacing(8);

    m_selectVersionButton = new AccentButton(
        QStringLiteral("选择版本"), Icons::Type::Gamepad, launchCard);
    m_selectVersionButton->setVariant(AccentButton::Variant::Secondary);
    m_selectVersionButton->setFont(Theme::font(13, QFont::DemiBold));
    m_selectVersionButton->setFixedHeight(46);
    m_selectVersionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_selectVersionButton, &QAbstractButton::clicked,
            this, &HomePage::chooseVersion);
    launchLayout->addWidget(m_selectVersionButton);

    overviewLayout->addWidget(launchCard, 2);
    auto *newsCard = new CardWidget(m_overviewPage);
    newsCard->setAccentColor(Theme::Blue);
    newsCard->setInteractive(true);
    newsCard->setCornerRadius(20);
    newsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *newsLayout = new QVBoxLayout(newsCard);
    newsLayout->setContentsMargins(24, 22, 24, 22);
    newsLayout->setSpacing(0);

    m_newsStack = new QStackedWidget(newsCard);
    m_newsStack->setFrameShape(QFrame::NoFrame);
    m_newsStack->setStyleSheet(QStringLiteral(
        "QStackedWidget { background: transparent; border: none; }"));

    m_newsWidget = new HomeNewsWidget(m_newsStack);
    m_statusWidget = new HomeStatusWidget(m_newsStack);

    m_blankNewsPage = new QWidget(m_newsStack);
    m_blankNewsPage->setAttribute(Qt::WA_NoSystemBackground);
    auto *blankLayout = new QVBoxLayout(m_blankNewsPage);
    blankLayout->setContentsMargins(0, 0, 0, 0);
    blankLayout->setSpacing(0);
    m_turtleAnimation = new TurtleAnimationWidget(m_blankNewsPage);
    m_turtleAnimation->setMinimumSize(320, 260);
    blankLayout->addWidget(m_turtleAnimation, 1);
    m_animePortrait = new AnimePortraitWidget(m_blankNewsPage);
    m_animePortrait->setMinimumSize(320, 260);
    m_animePortrait->hide();
    blankLayout->addWidget(m_animePortrait, 1);

    m_newsStack->addWidget(m_newsWidget);
    m_newsStack->addWidget(m_statusWidget);
    m_newsStack->addWidget(m_blankNewsPage);
    newsLayout->addWidget(m_newsStack, 1);

    overviewLayout->addWidget(newsCard, 5);

    m_versionPage = new VersionSelectPage(m_stack);
    m_stack->addPage(m_overviewPage);
    m_stack->addPage(m_versionPage);
    m_stack->setCurrentIndex(0, false);

    connect(m_versionPage, &VersionSelectPage::backRequested, this, [this]() {
        m_stack->setCurrentIndex(0, true);
    });
    connect(m_versionPage, &VersionSelectPage::versionSelected, this,
            [this](int index) {
        selectVersion(index);
        m_stack->setCurrentIndex(0, true);
    });
    connect(m_versionPage, &VersionSelectPage::detailsRequested,
            this, &HomePage::detailsRequested);
    connect(m_versionPage, &VersionSelectPage::deleteRequested,
            this, &HomePage::deleteVersion);

    refreshHomepageContent();
    reloadLocalVersions();
}

QString HomePage::gameDirectory() const
{
    return m_gameDirectory;
}

VersionInfo HomePage::versionAt(int versionIndex) const
{
    if (versionIndex < 0 || versionIndex >= m_versions.size()) {
        return VersionInfo();
    }
    return m_versions.at(versionIndex);
}

VersionInfo HomePage::selectedVersion() const
{
    return versionAt(m_selectedVersionIndex);
}

int HomePage::selectedVersionIndex() const
{
    return m_selectedVersionIndex;
}

void HomePage::setAnimationsEnabled(bool enabled)
{
    if (m_stack) {
        m_stack->setAnimationsEnabled(enabled);
    }
    if (m_turtleAnimation) {
        m_turtleAnimation->setAnimationsEnabled(enabled);
    }
    if (m_animePortrait) {
        m_animePortrait->setAnimationsEnabled(enabled);
    }
    if (m_newsWidget) {
        m_newsWidget->setAnimationsEnabled(enabled);
    }
    if (m_statusWidget) {
        m_statusWidget->setAnimationsEnabled(enabled);
    }
}

void HomePage::reloadLocalVersions()
{
    const QString previousVersionId = selectedVersion().versionId;
    const QString gameDirectory = m_gameDirectory;
    const quint64 generation = ++m_scanGeneration;
    m_versions.clear();
    m_selectedVersionIndex = -1;
    if (m_scanStatusLabel) {
        m_scanStatusLabel->setText(QStringLiteral("正在扫描本地版本…"));
        m_scanStatusLabel->show();
    }
    if (m_versionPage) {
        m_versionPage->setVersions(QVector<VersionInfo>(), -1, gameDirectory);
    }
    updateSelectedVersionUi();

    auto *watcher = new QFutureWatcher<LocalVersionScanner::Result>(this);
    connect(watcher, &QFutureWatcher<LocalVersionScanner::Result>::finished,
            this, [this, watcher, generation, previousVersionId]() {
        const LocalVersionScanner::Result result = watcher->result();
        watcher->deleteLater();
        if (generation != m_scanGeneration) {
            return;
        }

        m_versions = result.versions;
        m_selectedVersionIndex = m_versions.isEmpty() ? -1 : 0;
        for (int i = 0; i < m_versions.size(); ++i) {
            if (!previousVersionId.isEmpty()
                && m_versions.at(i).versionId == previousVersionId) {
                m_selectedVersionIndex = i;
                break;
            }
        }
        if (m_scanStatusLabel) {
            m_scanStatusLabel->setText(
                result.error.isEmpty()
                    ? QStringLiteral("%1 个版本已就绪").arg(m_versions.size())
                    : result.error);
            m_scanStatusLabel->setToolTip(
                result.error.isEmpty() ? QDir::toNativeSeparators(m_gameDirectory)
                                       : QString());
        }
        if (m_versionPage) {
            m_versionPage->setVersions(m_versions, m_selectedVersionIndex,
                                       m_gameDirectory);
        }
        updateSelectedVersionUi();
    });
    watcher->setFuture(QtConcurrent::run(&LocalVersionScanner::scan, gameDirectory));
}

void HomePage::openAccountDialog()
{
    QWidget *dialogParent = window() ? window() : this;
    AccountDialog dialog(dialogParent);
    dialog.exec();
}

void HomePage::setGameDirectory(const QString &directory)
{
    if (directory.trimmed().isEmpty()) {
        return;
    }
    const QString absolute = QDir(directory).absolutePath();
    if (absolute == m_gameDirectory) {
        return;
    }
    m_gameDirectory = absolute;
    MinecraftPaths::saveGameDirectory(m_gameDirectory);
    reloadLocalVersions();
}

void HomePage::chooseVersion()
{
    if (m_versions.isEmpty()) {
        emit notifyRequested(QStringLiteral("当前没有可选择的本地游戏版本。"));
        return;
    }
    m_stack->setCurrentIndex(1, true);
}

void HomePage::selectVersion(int index)
{
    if (index < 0 || index >= m_versions.size()) {
        return;
    }
    m_selectedVersionIndex = index;
    if (m_versionPage) {
        m_versionPage->setSelectedIndex(index);
    }
    updateSelectedVersionUi();
}

void HomePage::deleteVersion(int index)
{
    if (index < 0 || index >= m_versions.size()) {
        return;
    }
    const VersionInfo version = m_versions.at(index);
    const QDir versionsRoot(QDir(m_gameDirectory).filePath(QStringLiteral("versions")));
    QString versionPath = version.versionDirectory;
    if (versionPath.isEmpty()) {
        versionPath = versionsRoot.filePath(version.versionId);
    }

    const QString canonicalRoot = QFileInfo(versionsRoot.absolutePath()).canonicalFilePath();
    const QString canonicalVersion = QFileInfo(versionPath).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalVersion.isEmpty()) {
        emit notifyRequested(QStringLiteral("版本目录不存在，无法删除。"));
        return;
    }
    const QFileInfo versionInfo(canonicalVersion);
    if (!versionInfo.isDir()
        || QFileInfo(versionInfo.absolutePath()).canonicalFilePath() != canonicalRoot) {
        emit notifyRequested(QStringLiteral("版本目录不在当前游戏目录中，已拒绝删除。"));
        return;
    }

    const QString display = version.loader.isEmpty()
        ? displayVersionName(version)
        : QStringLiteral("%1 · %2").arg(displayVersionName(version), version.loader);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("删除版本"),
        QStringLiteral("确定要删除「%1」吗？\n\n"
                       "将删除版本目录 versions/%2。\n"
                       "该目录内的版本隔离 mods 也会一并删除；公共 libraries、assets 和根目录 mods 不会被删除。\n"
                       "此操作不可撤销。")
            .arg(display, versionInfo.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!version.loader.isEmpty()) {
        QString markerError;
        if (!markLoaderBaseVersion(m_gameDirectory, canonicalVersion, &markerError)) {
            QMessageBox::warning(
                this, QStringLiteral("删除失败"),
                QStringLiteral("无法标记加载器依赖的基础版本，因此未删除该版本：\n%1")
                    .arg(markerError));
            return;
        }
    }

    if (!QDir(canonicalVersion).removeRecursively()) {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
                             QStringLiteral("无法完整删除该版本，可能有文件正在被游戏占用。"));
        return;
    }

    reloadLocalVersions();
    emit notifyRequested(QStringLiteral("已删除版本「%1」。").arg(display));
}

void HomePage::updateSelectedVersionUi()
{
    const VersionInfo version = selectedVersion();
    const bool hasVersion = !version.minecraft.isEmpty();
    const QString name = hasVersion ? displayVersionName(version)
                                    : QStringLiteral("未选择版本");

    if (m_versionNameLabel) {
        m_versionNameLabel->setText(name);
    }
    if (m_versionMetaLabel) {
        m_versionMetaLabel->setText(
            hasVersion
                ? QStringLiteral("Minecraft %1%2")
                      .arg(version.minecraft,
                           version.loader.isEmpty()
                               ? QString()
                               : QStringLiteral("  ·  %1 %2")
                                     .arg(version.loader, version.loaderVersion))
                : QStringLiteral("请先选择一个本地游戏版本。"));
    }
    if (m_versionStatsLabel) {
        m_versionStatsLabel->setText(
            hasVersion
                ? QStringLiteral("%1 个 Mod  ·  已安装")
                      .arg(version.mods.size())
                : QString());
        m_versionStatsLabel->setToolTip(
            hasVersion ? QDir::toNativeSeparators(version.versionDirectory) : QString());
    }
    if (m_launchButton) {
        m_launchButton->setText(hasVersion
                                    ? QStringLiteral("启动游戏  ·  %1")
                                          .arg(truncatedName(name))
                                    : QStringLiteral("启动游戏"));
        m_launchButton->setEnabled(hasVersion);
        m_launchButton->setToolTip(hasVersion ? name : QString());
    }
    if (m_selectVersionButton) {
        m_selectVersionButton->setEnabled(!m_versions.isEmpty());
    }
    if (m_versionPage) {
        m_versionPage->setSelectedIndex(m_selectedVersionIndex);
    }
}

void HomePage::refreshHomepageContent()
{
    if (!m_newsStack) {
        return;
    }

    const GlobalSettings settings = GlobalSettings::load();
    const QString mode = settings.homepage.mode;

    if (mode == QStringLiteral("blank")) {
        m_newsStack->setCurrentWidget(m_blankNewsPage);
        const bool animate = settings.homepage.blankAnimation;
        const bool live2d = settings.homepage.blankAnimationStyle
            .startsWith(QStringLiteral("live2d"));
        if (m_turtleAnimation) {
            m_turtleAnimation->setVisible(animate && !live2d);
            m_turtleAnimation->setActive(animate && !live2d);
        }
        if (m_animePortrait) {
            if (live2d) {
                m_animePortrait->setMotionParameters(
                    settings.homepage.live2dBreathAmplitude,
                    settings.homepage.live2dSwayAmplitude,
                    settings.homepage.live2dEyeAmplitude);
                m_animePortrait->setPortrait(portraitResourceForStyle(
                    settings.homepage.blankAnimationStyle));
            }
            m_animePortrait->setVisible(animate && live2d);
            m_animePortrait->setActive(animate && live2d);
        }
        return;
    }

    if (m_turtleAnimation) {
        m_turtleAnimation->setActive(false);
        m_turtleAnimation->hide();
    }
    if (m_animePortrait) {
        m_animePortrait->setActive(false);
        m_animePortrait->hide();
    }
    if (mode == QStringLiteral("status")) {
        m_newsStack->setCurrentWidget(m_statusWidget);
    } else {
        m_newsStack->setCurrentWidget(m_newsWidget);
    }
}
