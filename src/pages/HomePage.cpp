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
#include "Theme.h"
#include "VersionCard.h"
#include "account/AccountDialog.h"
#include "account/AccountManager.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTime>
#include <QVBoxLayout>

namespace {

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

QString greetingForNow()
{
    const int hour = QTime::currentTime().hour();
    if (hour < 6) {
        return QStringLiteral("夜深了");
    }
    if (hour < 12) {
        return QStringLiteral("早上好");
    }
    if (hour < 18) {
        return QStringLiteral("下午好");
    }
    return QStringLiteral("晚上好");
}

QWidget *statPill(Icons::Type icon, const QString &text, QLabel **valueLabel,
                  QWidget *parent)
{
    auto *widget = new QWidget(parent);
    widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *iconLabel = new QLabel(widget);
    iconLabel->setPixmap(Icons::icon(icon, Theme::TextMuted, 13).pixmap(13, 13));
    iconLabel->setFixedSize(13, 13);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(iconLabel);
    auto *textLabel = Theme::label(text, 11, QFont::Normal, Theme::TextSecondary, widget);
    if (valueLabel) {
        *valueLabel = textLabel;
    }
    layout->addWidget(textLabel);
    return widget;
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

    auto *shell = new QHBoxLayout(this);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("homeSidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setStyleSheet(QStringLiteral(
        "QWidget#homeSidebar { background: %1; border-right: 1px solid rgba(%2, %3, %4, %5); }")
        .arg(Theme::Surface.name())
        .arg(Theme::Border.red())
        .arg(Theme::Border.green())
        .arg(Theme::Border.blue())
        .arg(Theme::Border.alpha()));
    sidebar->setFixedWidth(360);
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
    side->setContentsMargins(30, 24, 30, 20);
    side->setSpacing(0);
    sideScroll->setWidget(sideContent);
    sidebarLayout->addWidget(sideScroll);

    auto *modeRow = new QHBoxLayout();
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(8);
    modeRow->addWidget(Theme::label(QStringLiteral("本地账户"), 11, QFont::DemiBold,
                                    Theme::TextSecondary, sidebar));
    modeRow->addStretch();
    modeRow->addWidget(Theme::badge(QStringLiteral("离线"), Theme::Accent,
                                    QColor(47, 128, 237, 20), sidebar,
                                    QColor(47, 128, 237, 58)));
    side->addLayout(modeRow);
    side->addSpacing(46);

    auto *avatarRow = new QHBoxLayout();
    avatarRow->setContentsMargins(0, 0, 0, 0);
    avatarRow->addStretch();
    auto *avatar = new AvatarWidget(96, sidebar);
    avatar->setOnline(false);
    avatarRow->addWidget(avatar);
    avatarRow->addStretch();
    side->addLayout(avatarRow);
    side->addSpacing(16);

    m_accountNameLabel = Theme::label(QStringLiteral("未添加账户"), 23, QFont::DemiBold,
                                       Theme::TextPrimary, sidebar, true);
    m_accountNameLabel->setAlignment(Qt::AlignCenter);
    side->addWidget(m_accountNameLabel);
    side->addSpacing(4);

    auto *accountBadgeRow = new QHBoxLayout();
    accountBadgeRow->setContentsMargins(0, 0, 0, 0);
    accountBadgeRow->addStretch();
    m_accountTypeBadge = Theme::badge(QStringLiteral("离线账户"), Theme::Blue,
                                      QColor(47, 128, 237, 22), sidebar,
                                      QColor(47, 128, 237, 70));
    accountBadgeRow->addWidget(m_accountTypeBadge);
    accountBadgeRow->addStretch();
    side->addLayout(accountBadgeRow);
    side->addSpacing(14);

    m_accountButton = new AccentButton(QStringLiteral("切换账户"), Icons::Type::ChevronDown,
                                       sidebar);
    m_accountButton->setVariant(AccentButton::Variant::Secondary);
    m_accountButton->setFixedHeight(42);
    m_accountButton->setToolTip(QStringLiteral("添加、修改或删除本地账户"));
    m_accountButton->setAccessibleName(QStringLiteral("管理本地账户"));
    connect(m_accountButton, &QAbstractButton::clicked,
            this, &HomePage::openAccountDialog);
    side->addWidget(m_accountButton);
    side->addSpacing(18);

    auto *statsRow = new QHBoxLayout();
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(16);
    statsRow->addWidget(statPill(Icons::Type::Cube, QStringLiteral("0 个模组"),
                                 &m_heroModsLabel, sidebar));
    statsRow->addWidget(statPill(Icons::Type::Grid, QStringLiteral("0 个本地版本"),
                                 &m_heroVersionsLabel, sidebar));
    statsRow->addStretch();
    side->addLayout(statsRow);

    side->addStretch();
    side->addWidget(Theme::label(QStringLiteral("当前版本"), 10, QFont::Normal,
                                 Theme::TextMuted, sidebar));
    side->addSpacing(2);
    m_recentVersionLabel = Theme::label(QStringLiteral("未检测到版本"), 16, QFont::DemiBold,
                                        Theme::TextPrimary, sidebar);
    side->addWidget(m_recentVersionLabel);
    side->addSpacing(10);

    m_launchButton = new AccentButton(QStringLiteral("启动游戏"), Icons::Type::Play, sidebar);
    m_launchButton->setVariant(AccentButton::Variant::Primary);
    m_launchButton->setFont(Theme::font(15, QFont::DemiBold));
    m_launchButton->setFixedHeight(58);
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
    root->setContentsMargins(34, 26, 34, 18);
    root->setSpacing(0);

    auto *greetingRow = new QHBoxLayout();
    greetingRow->setContentsMargins(0, 0, 0, 0);
    greetingRow->setSpacing(12);

    auto *greetingText = new QVBoxLayout();
    greetingText->setContentsMargins(0, 0, 0, 0);
    greetingText->setSpacing(3);
    auto *eyebrow = Theme::label(QStringLiteral("启动  /  LAUNCH"), 9, QFont::Bold,
                                 Theme::Accent, content);
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    eyebrow->setFont(eyebrowFont);
    greetingText->addWidget(eyebrow);
    m_greetingLabel = Theme::label(QStringLiteral("%1，冒险家").arg(greetingForNow()),
                                   27, QFont::DemiBold, Theme::TextPrimary, content, true);
    greetingText->addWidget(m_greetingLabel);
    greetingText->addWidget(Theme::label(QStringLiteral("选择一个本地版本，开始新的冒险。"),
                                         12, QFont::Normal, Theme::TextMuted, content));
    greetingRow->addLayout(greetingText);
    greetingRow->addStretch();

    auto *dateLayout = new QVBoxLayout();
    dateLayout->setContentsMargins(0, 2, 0, 0);
    dateLayout->setSpacing(6);
    const QLocale locale(QLocale::Chinese, QLocale::China);
    auto *dateLabel = Theme::label(
        locale.toString(QDate::currentDate(), QStringLiteral("yyyy 年 M 月 d 日 · dddd")),
        11, QFont::Normal, Theme::TextMuted, content);
    dateLabel->setAlignment(Qt::AlignRight);
    dateLayout->addWidget(dateLabel);
    greetingRow->addLayout(dateLayout);
    root->addLayout(greetingRow);
    root->addSpacing(22);

    auto *sectionRow = new QHBoxLayout();
    sectionRow->setContentsMargins(0, 0, 0, 0);
    sectionRow->setSpacing(10);
    auto *sectionText = new QVBoxLayout();
    sectionText->setContentsMargins(0, 0, 0, 0);
    sectionText->setSpacing(2);
    sectionText->addWidget(Theme::label(QStringLiteral("本地游戏版本"), 17, QFont::DemiBold,
                                        Theme::TextPrimary, content));
    sectionText->addWidget(Theme::label(QStringLiteral("点击卡片选择要启动的版本。"),
                                        11, QFont::Normal, Theme::TextMuted, content));
    sectionRow->addLayout(sectionText);
    sectionRow->addStretch();
    m_versionCountBadge = Theme::badge(QStringLiteral("0 个版本"),
                                       Theme::Accent, QColor(47, 128, 237, 18),
                                       content, QColor(47, 128, 237, 55));
    sectionRow->addWidget(m_versionCountBadge, 0, Qt::AlignVCenter);
    root->addLayout(sectionRow);
    root->addSpacing(12);

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
    root->addSpacing(12);

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
    const LocalVersionScanner::Result result =
        LocalVersionScanner::scan(m_gameDirectory);
    m_versions = result.versions;
    m_mods = result.mods;

    if (m_emptyVersionsLabel) {
        m_emptyVersionsLabel->setText(
            result.error.isEmpty()
                ? QStringLiteral("当前目录下没有发现已安装版本。")
                : result.error);
    }

    if (m_heroVersionsLabel) {
        m_heroVersionsLabel->setText(
            QStringLiteral("%1 个本地版本").arg(m_versions.size()));
    }
    if (m_heroModsLabel) {
        m_heroModsLabel->setText(QStringLiteral("%1 个模组").arg(m_mods.size()));
    }
    if (m_versionCountBadge) {
        m_versionCountBadge->setText(QStringLiteral("%1 个版本").arg(m_versions.size()));
    }

    m_selectedVersionIndex = m_versions.isEmpty() ? -1 : 0;
    updatePathDisplay();
    rebuildVersionCards();
    updateSelectedVersionUi();
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

    m_greetingLabel->setText(
        QStringLiteral("%1，%2")
            .arg(greetingForNow(), hasAccount ? account.username : QStringLiteral("冒险家")));
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
        QStringLiteral("background: rgba(%1, %2, %3, 28);"
                       "color: %4;"
                       "border: 1px solid rgba(%1, %2, %3, 72);"
                       "border-radius: 8px; padding: 4px 9px;")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
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
        ? version.minecraft
        : QStringLiteral("%1 · %2").arg(version.minecraft, version.loader);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("删除版本"),
        QStringLiteral("确定要删除「%1」吗？\n\n"
                       "将删除版本目录 versions/%2。\n"
                       "公共 libraries、assets 和根目录 mods 不会被删除。\n"
                       "此操作不可撤销。")
            .arg(display, versionInfo.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
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
    if (!m_recentVersionLabel) {
        return;
    }
    const VersionInfo version = selectedVersion();
    if (version.minecraft.isEmpty()) {
        m_recentVersionLabel->setText(QStringLiteral("未检测到版本"));
        if (m_launchButton) {
            m_launchButton->setText(QStringLiteral("启动游戏"));
            m_launchButton->setEnabled(false);
        }
        return;
    }

    m_recentVersionLabel->setText(
        version.loader.isEmpty()
            ? version.minecraft
            : QStringLiteral("%1  ·  %2").arg(version.minecraft, version.loader));
    if (m_launchButton) {
        m_launchButton->setText(
            QStringLiteral("启动游戏  ·  %1").arg(version.minecraft));
        m_launchButton->setEnabled(true);
    }
}
