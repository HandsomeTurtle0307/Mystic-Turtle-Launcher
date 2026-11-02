/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "HomePage.h"
#include "LocalVersionScanner.h"
#include "MinecraftPaths.h"

#include "AccentButton.h"
#include "AvatarWidget.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "LoadingIndicator.h"
#include "Theme.h"
#include "VersionCard.h"
#include "account/AccountDialog.h"
#include "account/AccountManager.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace {

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

class HeroPanel : public CardWidget
{
public:
    explicit HeroPanel(QWidget *parent = nullptr)
        : CardWidget(parent)
    {
        setAccentColor(Theme::Accent);
        setCornerRadius(20.0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        CardWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            cornerRadius(), cornerRadius());
        painter.setClipPath(clip);

        const QPointF center(width() - 108.0, height() / 2.0 - 6.0);
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 4; ++i) {
            const qreal radius = 42.0 + i * 31.0;
            painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                       Theme::Accent.blue(), 20 - i * 4),
                                1.0));
            painter.drawEllipse(center, radius, radius);
        }

        painter.setPen(QPen(QColor(47, 128, 237, 55), 1.1));
        painter.drawArc(QRectF(center.x() - 104, center.y() - 104, 208, 208),
                        -62 * 16, 86 * 16);
        painter.setPen(QPen(QColor(47, 128, 237, 38), 1.0));
        painter.drawArc(QRectF(center.x() - 135, center.y() - 135, 270, 270),
                        -96 * 16, 110 * 16);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                Theme::Accent.blue(), 75));
        painter.drawEllipse(QPointF(width() - 196, 31), 2.2, 2.2);
        painter.drawEllipse(QPointF(width() - 48, height() - 40), 1.6, 1.6);

        QLinearGradient fade(0, height() - 56, 0, height());
        fade.setColorAt(0.0, QColor(47, 128, 237, 0));
        fade.setColorAt(1.0, QColor(47, 128, 237, 10));
        painter.fillRect(QRectF(0, height() - 56, width(), 56), fade);
    }
};

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

    auto *shell = new QHBoxLayout(this);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("homeSidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setStyleSheet(QStringLiteral(
        "QWidget#homeSidebar { background: transparent; border-right: 1px solid rgba(%1, %2, %3, %4); }")
        .arg(Theme::Border.red())
        .arg(Theme::Border.green())
        .arg(Theme::Border.blue())
        .arg(Theme::Border.alpha()));
    sidebar->setFixedWidth(300);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto *sideScroll = new QScrollArea(sidebar);
    sideScroll->setWidgetResizable(true);
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    sideScroll->viewport()->setAutoFillBackground(false);

    auto *sideContent = new QWidget();
    sideContent->setAttribute(Qt::WA_NoSystemBackground);
    auto *side = new QVBoxLayout(sideContent);
    side->setContentsMargins(22, 18, 22, 18);
    side->setSpacing(0);
    sideScroll->setWidget(sideContent);
    sidebarLayout->addWidget(sideScroll);

    auto *accountRow = new QHBoxLayout();
    accountRow->setContentsMargins(0, 0, 0, 0);
    accountRow->setSpacing(14);
    auto *avatar = new AvatarWidget(56, sidebar);
    avatar->setOnline(false);
    accountRow->addWidget(avatar, 0, Qt::AlignVCenter);

    auto *accountInfo = new QVBoxLayout();
    accountInfo->setContentsMargins(0, 0, 0, 0);
    accountInfo->setSpacing(5);
    m_accountNameLabel = Theme::label(QStringLiteral("未添加账户"), 17, QFont::DemiBold,
                                      Theme::TextPrimary, sidebar, true);
    m_accountNameLabel->setMinimumWidth(0);
    m_accountNameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    accountInfo->addWidget(m_accountNameLabel);
    m_accountTypeBadge = Theme::badge(QStringLiteral("离线账户"), Theme::Blue,
                                      QColor(47, 128, 237, 22), sidebar,
                                      QColor(47, 128, 237, 70));
    accountInfo->addWidget(m_accountTypeBadge, 0, Qt::AlignLeft);
    accountRow->addLayout(accountInfo, 1);
    side->addLayout(accountRow);
    side->addSpacing(16);

    m_accountButton = new AccentButton(QStringLiteral("切换账户"), Icons::Type::ChevronDown,
                                       sidebar);
    m_accountButton->setVariant(AccentButton::Variant::Secondary);
    m_accountButton->setFixedHeight(38);
    m_accountButton->setToolTip(QStringLiteral("添加、修改或删除本地账户"));
    m_accountButton->setAccessibleName(QStringLiteral("管理本地账户"));
    connect(m_accountButton, &QAbstractButton::clicked,
            this, &HomePage::openAccountDialog);
    side->addWidget(m_accountButton);
    side->addSpacing(12);
    auto *accountDivider = new QFrame(sidebar);
    accountDivider->setFrameShape(QFrame::HLine);
    accountDivider->setFrameShadow(QFrame::Plain);
    accountDivider->setStyleSheet(QStringLiteral("color: %1;")
                                      .arg(Theme::Border.name()));
    side->addWidget(accountDivider);

    side->addStretch();

    m_launchButton = new AccentButton(QStringLiteral("启动游戏"), Icons::Type::Play, sidebar);
    m_launchButton->setVariant(AccentButton::Variant::Primary);
    m_launchButton->setFont(Theme::font(15, QFont::DemiBold));
    m_launchButton->setFixedHeight(50);
    m_launchButton->setAccessibleName(QStringLiteral("启动当前选择的游戏版本"));
    connect(m_launchButton, &QAbstractButton::clicked, this, [this]() {
        if (m_selectedVersionIndex < 0
            || m_selectedVersionIndex >= m_versions.size()) {
            emit notifyRequested(QStringLiteral("请先选择一个本地游戏版本。"));
            return;
        }
        emit launchRequested();
    });
    side->addWidget(m_launchButton);

    shell->addWidget(sidebar);

    auto *content = new QWidget(this);
    content->setAttribute(Qt::WA_NoSystemBackground);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(28, 22, 28, 16);
    root->setSpacing(0);

    auto *sectionRow = new QHBoxLayout();
    sectionRow->setContentsMargins(0, 0, 0, 0);
    sectionRow->setSpacing(10);
    sectionRow->addWidget(Theme::label(QStringLiteral("游戏版本"), 22, QFont::DemiBold,
                                       Theme::TextPrimary, content));
    sectionRow->addStretch();
    m_versionCountBadge = Theme::badge(QStringLiteral("0 个版本"),
                                       Theme::Accent, QColor(47, 128, 237, 18),
                                       content, QColor(47, 128, 237, 55));
    m_scanLoadingIndicator = new LoadingIndicator(content, Theme::Accent);
    m_scanLoadingIndicator->setFixedSize(16, 16);
    m_scanLoadingIndicator->hide();
    m_scanStatusLabel = Theme::label(QStringLiteral("正在扫描本地版本…"), 10,
                                     QFont::Normal, Theme::TextMuted, content);
    m_scanStatusLabel->hide();
    sectionRow->addWidget(m_scanLoadingIndicator, 0, Qt::AlignVCenter);
    sectionRow->addWidget(m_scanStatusLabel, 0, Qt::AlignVCenter);
    sectionRow->addWidget(m_versionCountBadge, 0, Qt::AlignVCenter);
    root->addLayout(sectionRow);
    root->addSpacing(16);

    auto *versionScroll = new QScrollArea(content);
    versionScroll->setWidgetResizable(true);
    versionScroll->setFrameShape(QFrame::NoFrame);
    versionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    versionScroll->viewport()->setAutoFillBackground(false);

    auto *versionContent = new QWidget();
    versionContent->setAttribute(Qt::WA_NoSystemBackground);
    auto *versionContentLayout = new QVBoxLayout(versionContent);
    versionContentLayout->setContentsMargins(0, 0, 7, 4);
    versionContentLayout->setSpacing(0);

    m_versionGrid = new QGridLayout();
    m_versionGrid->setContentsMargins(0, 0, 0, 0);
    m_versionGrid->setHorizontalSpacing(14);
    m_versionGrid->setVerticalSpacing(12);
    versionContentLayout->addLayout(m_versionGrid);

    m_emptyVersionsLabel = Theme::label(
        QStringLiteral("当前目录下没有发现已安装版本。"), 12,
        QFont::Normal, Theme::TextMuted, versionContent);
    m_emptyVersionsLabel->setAlignment(Qt::AlignCenter);
    m_emptyVersionsLabel->setFixedHeight(120);
    m_emptyVersionsLabel->hide();
    versionContentLayout->addWidget(m_emptyVersionsLabel);
    versionContentLayout->addStretch(1);

    versionScroll->setWidget(versionContent);
    root->addWidget(versionScroll, 1);
    root->addSpacing(8);

    auto *pathRow = new QHBoxLayout();
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(10);
    auto *choosePathButton = new AccentButton(QStringLiteral("游戏目录"),
                                              Icons::Type::Folder, content);
    choosePathButton->setVariant(AccentButton::Variant::Ghost);
    choosePathButton->setCompact(true);
    connect(choosePathButton, &QAbstractButton::clicked,
            this, &HomePage::chooseGameDirectory);
    pathRow->addWidget(choosePathButton);

    m_gamePathLabel = Theme::label(QString(), 10, QFont::Normal,
                                   Theme::TextMuted, content);
    m_gamePathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    pathRow->addWidget(m_gamePathLabel, 1);
    root->addLayout(pathRow);

    shell->addWidget(content, 1);

    reloadLocalVersions();

    connect(&AccountManager::instance(), &AccountManager::accountChanged,
            this, &HomePage::updateAccountUi);
    updateAccountUi();
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

void HomePage::reloadLocalVersions()
{
    const QString gameDirectory = m_gameDirectory;
    const quint64 generation = ++m_scanGeneration;
    m_versions.clear();
    m_selectedVersionIndex = -1;
    m_versionCountBadge->setText(QStringLiteral("正在扫描…"));
    m_scanLoadingIndicator->show();
    m_scanStatusLabel->show();
    m_emptyVersionsLabel->setText(QStringLiteral("正在扫描本地游戏版本…"));
    m_emptyVersionsLabel->show();
    rebuildVersionCards();
    updateSelectedVersionUi();

    auto *watcher = new QFutureWatcher<LocalVersionScanner::Result>(this);
    connect(watcher, &QFutureWatcher<LocalVersionScanner::Result>::finished,
            this, [this, watcher, generation]() {
        const LocalVersionScanner::Result result = watcher->result();
        watcher->deleteLater();
        if (generation != m_scanGeneration) {
            return;
        }

        m_scanLoadingIndicator->hide();
        m_scanStatusLabel->hide();
        m_versions = result.versions;
        if (m_emptyVersionsLabel) {
            m_emptyVersionsLabel->setText(
                result.error.isEmpty()
                    ? QStringLiteral("当前目录下没有发现已安装版本。")
                    : result.error);
        }
        m_versionCountBadge->setText(
            QStringLiteral("%1 个版本").arg(m_versions.size()));
        m_selectedVersionIndex = m_versions.isEmpty() ? -1 : 0;
        updatePathDisplay();
        rebuildVersionCards();
        updateSelectedVersionUi();
    });
    watcher->setFuture(QtConcurrent::run(&LocalVersionScanner::scan, gameDirectory));
}

void HomePage::chooseGameDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 .minecraft 游戏目录"), m_gameDirectory);
    if (directory.isEmpty()) {
        return;
    }

    m_gameDirectory = QDir(directory).absolutePath();
    MinecraftPaths::saveGameDirectory(m_gameDirectory);
    emit gameDirectoryChanged(m_gameDirectory);
    reloadLocalVersions();
}

void HomePage::updatePathDisplay()
{
    if (!m_gamePathLabel) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(m_gameDirectory);
    QString displayPath = nativePath;
    if (displayPath.size() > 68) {
        displayPath = displayPath.left(18) + QStringLiteral("…")
            + displayPath.right(46);
    }
    m_gamePathLabel->setText(QStringLiteral("当前路径：%1").arg(displayPath));
    m_gamePathLabel->setToolTip(nativePath);
}

void HomePage::updateAccountUi()
{
    const PlayerAccount account = AccountManager::instance().account();
    const bool hasAccount = account.isValid();

    m_accountNameLabel->setText(hasAccount ? account.username
                                           : QStringLiteral("未添加账户"));
    m_accountButton->setText(hasAccount ? QStringLiteral("切换账户")
                                        : QStringLiteral("添加账户"));
    m_accountTypeBadge->setVisible(hasAccount);

    if (!hasAccount) {
        return;
    }

    m_accountTypeBadge->setText(account.typeLabel());
    const QColor color = account.type == PlayerAccount::Type::Premium
        ? Theme::Amber
        : Theme::Blue;
    m_accountTypeBadge->setStyleSheet(
        QStringLiteral("background: transparent; color: %1; border: none; padding: 4px 9px;")
            .arg(color.name()));
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
    updatePathDisplay();
    reloadLocalVersions();
}

void HomePage::rebuildVersionCards()
{
    if (!m_versionGrid) {
        return;
    }

    m_versionCards.clear();
    while (QLayoutItem *item = m_versionGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    QWidget *cardsParent = m_versionGrid->parentWidget();
    if (!cardsParent) {
        cardsParent = this;
    }

    for (int i = 0; i < m_versions.size(); ++i) {
        auto *card = new VersionCard(m_versions.at(i), i, cardsParent);
        connect(card, &VersionCard::detailsRequested,
                this, &HomePage::detailsRequested);
        connect(card, &VersionCard::deleteRequested,
                this, &HomePage::deleteVersion);
        connect(card, &VersionCard::selected, this, &HomePage::selectVersion);
        card->setSelected(i == m_selectedVersionIndex);
        m_versionCards.append(card);
        m_versionGrid->addWidget(card, i / 3, i % 3);
    }
    for (int column = 0; column < 3; ++column) {
        m_versionGrid->setColumnStretch(column, 1);
    }

    if (m_emptyVersionsLabel) {
        m_emptyVersionsLabel->setVisible(m_versions.isEmpty());
    }
}

void HomePage::selectVersion(int index)
{
    if (index < 0 || index >= m_versions.size()) {
        return;
    }
    m_selectedVersionIndex = index;
    for (int i = 0; i < m_versionCards.size(); ++i) {
        m_versionCards.at(i)->setSelected(i == index);
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
        ? (version.displayName.isEmpty() ? version.minecraft : version.displayName)
        : QStringLiteral("%1 · %2")
              .arg(version.displayName.isEmpty() ? version.minecraft : version.displayName,
                   version.loader);
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
    if (m_launchButton) {
        m_launchButton->setText(version.minecraft.isEmpty()
                                    ? QStringLiteral("启动游戏")
                                    : QStringLiteral("启动游戏  ·  %1")
                                          .arg(version.displayName.isEmpty()
                                                   ? version.minecraft
                                                   : version.displayName));
        m_launchButton->setEnabled(!version.minecraft.isEmpty());
    }
}
