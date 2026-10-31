/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "SettingsPage.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "MinecraftPaths.h"
#include "Theme.h"
#include "ToggleSwitch.h"
#include "network/MojangMirror.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QLabel *iconTile(Icons::Type type, const QColor &accent, QWidget *parent)
{
    auto *tile = new QLabel(parent);
    tile->setFixedSize(36, 36);
    tile->setAlignment(Qt::AlignCenter);
    tile->setPixmap(Icons::icon(type, accent, 17).pixmap(17, 17));
    tile->setAttribute(Qt::WA_TransparentForMouseEvents);
    tile->setStyleSheet(QStringLiteral(
                            "background: rgba(%1, %2, %3, 24);"
                            "border: 1px solid rgba(%1, %2, %3, 46);"
                            "border-radius: 10px;")
                            .arg(accent.red())
                            .arg(accent.green())
                            .arg(accent.blue()));
    return tile;
}

QWidget *settingRow(Icons::Type icon,
                    const QString &title,
                    const QString &description,
                    QWidget *control,
                    QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(iconTile(icon, Theme::Accent, row), 0, Qt::AlignTop);

    auto *text = new QVBoxLayout();
    text->setContentsMargins(0, 1, 0, 0);
    text->setSpacing(1);
    text->addWidget(Theme::label(title, 12, QFont::DemiBold, Theme::TextPrimary, row));
    text->addWidget(Theme::label(description, 10, QFont::Normal, Theme::TextMuted, row));
    layout->addLayout(text, 1);

    if (control) {
        layout->addWidget(control, 0, Qt::AlignVCenter);
    }
    return row;
}

QWidget *pathBlock(Icons::Type icon,
                   const QString &title,
                   const QString &description,
                   QLineEdit *edit,
                   AccentButton *browse,
                   QWidget *parent)
{
    auto *block = new QWidget(parent);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    auto *head = new QHBoxLayout();
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(12);
    head->addWidget(iconTile(icon, Theme::Accent, block), 0, Qt::AlignTop);
    auto *text = new QVBoxLayout();
    text->setContentsMargins(0, 1, 0, 0);
    text->setSpacing(1);
    text->addWidget(Theme::label(title, 12, QFont::DemiBold, Theme::TextPrimary, block));
    text->addWidget(Theme::label(description, 10, QFont::Normal, Theme::TextMuted, block));
    head->addLayout(text, 1);
    layout->addLayout(head);

    auto *inputRow = new QHBoxLayout();
    inputRow->setContentsMargins(48, 0, 0, 0);
    inputRow->setSpacing(8);
    edit->setFixedHeight(38);
    edit->setFont(Theme::font(11));
    inputRow->addWidget(edit, 1);
    browse->setCompact(true);
    browse->setVariant(AccentButton::Variant::Ghost);
    inputRow->addWidget(browse);
    layout->addLayout(inputRow);
    return block;
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    const QSettings initialSettings;
    const bool acrylicEnabled =
        initialSettings.value(QStringLiteral("ui/acrylic"), false).toBool();
    const bool animationsEnabled =
        initialSettings.value(QStringLiteral("ui/animations"), true).toBool();
    const int memoryGb = qBound(
        2, initialSettings.value(QStringLiteral("game/memoryGb"), 8).toInt(), 16);
    const QString savedJavaPath =
        initialSettings.value(QStringLiteral("game/javaPath")).toString();
    const QString downloadSource =
        initialSettings.value(QStringLiteral("network/downloadSource"),
                              QStringLiteral("bmclapi")).toString();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 22, 32, 16);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);
    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(3);
    auto *eyebrow = Theme::label(QStringLiteral("偏好  /  PREFERENCES"), 9, QFont::Bold,
                                 Theme::Accent, this);
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    eyebrow->setFont(eyebrowFont);
    headerText->addWidget(eyebrow);
    headerText->addWidget(Theme::label(QStringLiteral("设置"), 27, QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    headerText->addWidget(Theme::label(QStringLiteral("调整启动器的外观与本地运行环境。"),
                                       12, QFont::Normal, Theme::TextMuted, this));
    header->addLayout(headerText);
    header->addStretch();
    header->addWidget(Theme::badge(QStringLiteral("设置会自动保存"), Theme::Accent,
                                   QColor(Theme::Accent.red(), Theme::Accent.green(),
                                          Theme::Accent.blue(), 18),
                                   this, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                                Theme::Accent.blue(), 45)),
                      0, Qt::AlignVCenter);
    root->addLayout(header);
    root->addSpacing(16);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget();
    content->setAttribute(Qt::WA_NoSystemBackground);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 7, 4);
    contentLayout->setSpacing(14);

    auto *cardsRow = new QHBoxLayout();
    cardsRow->setContentsMargins(0, 0, 0, 0);
    cardsRow->setSpacing(14);

    // Appearance
    auto *appearance = new CardWidget(content);
    appearance->setAccentColor(Theme::Accent);
    appearance->setMinimumHeight(360);
    appearance->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *appearanceLayout = new QVBoxLayout(appearance);
    appearanceLayout->setContentsMargins(22, 19, 22, 19);
    appearanceLayout->setSpacing(0);
    appearanceLayout->addWidget(Theme::label(QStringLiteral("外观"), 17, QFont::DemiBold,
                                             Theme::TextPrimary, appearance));
    appearanceLayout->addWidget(Theme::label(QStringLiteral("界面材质、主题与过渡动效。"),
                                             10, QFont::Normal, Theme::TextMuted, appearance));
    appearanceLayout->addSpacing(12);
    appearanceLayout->addWidget(Theme::divider(appearance));
    appearanceLayout->addSpacing(12);

    auto *acrylicSwitch = new ToggleSwitch(appearance);
    acrylicSwitch->setChecked(acrylicEnabled);
    acrylicSwitch->setAccessibleName(QStringLiteral("Acrylic 毛玻璃"));
    connect(acrylicSwitch, &QAbstractButton::toggled, this, &SettingsPage::acrylicEnabledChanged);
    appearanceLayout->addWidget(settingRow(
        Icons::Type::Sparkles, QStringLiteral("Acrylic 毛玻璃"),
        QStringLiteral("使用 Windows 11 DWM 背景材质与柔和着色。"),
        acrylicSwitch, appearance));
    appearanceLayout->addSpacing(11);

    auto *animationsSwitch = new ToggleSwitch(appearance);
    animationsSwitch->setChecked(animationsEnabled);
    animationsSwitch->setAccessibleName(QStringLiteral("丝滑动画"));
    connect(animationsSwitch, &QAbstractButton::toggled,
            this, &SettingsPage::animationsEnabledChanged);
    appearanceLayout->addWidget(settingRow(
        Icons::Type::Refresh, QStringLiteral("丝滑动画"),
        QStringLiteral("页面切换、导航指示器与悬停过渡。"),
        animationsSwitch, appearance));
    appearanceLayout->addSpacing(11);

    appearanceLayout->addStretch();

    // Game environment
    auto *game = new CardWidget(content);
    game->setAccentColor(Theme::Blue);
    game->setMinimumHeight(360);
    game->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *gameLayout = new QVBoxLayout(game);
    gameLayout->setContentsMargins(22, 19, 22, 19);
    gameLayout->setSpacing(0);
    gameLayout->addWidget(Theme::label(QStringLiteral("游戏与运行环境"), 17, QFont::DemiBold,
                                       Theme::TextPrimary, game));
    gameLayout->addWidget(Theme::label(QStringLiteral("Java 路径、内存上限与本地游戏目录。"),
                                       10, QFont::Normal, Theme::TextMuted, game));
    gameLayout->addSpacing(12);
    gameLayout->addWidget(Theme::divider(game));
    gameLayout->addSpacing(12);

    m_javaEdit = new QLineEdit(game);
    m_javaEdit->setReadOnly(true);
    m_javaEdit->setPlaceholderText(QStringLiteral("自动检测 Java"));
    if (!savedJavaPath.trimmed().isEmpty()) {
        m_javaEdit->setText(QDir::toNativeSeparators(savedJavaPath));
    }
    auto *javaBrowse = new AccentButton(QStringLiteral("浏览"), Icons::Type::Folder, game);
    connect(javaBrowse, &QAbstractButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择 Java 可执行文件"), QString(),
            QStringLiteral("Java 可执行文件 (java.exe java);;所有文件 (*)"));
        if (!file.isEmpty()) {
            m_javaEdit->setText(QDir::toNativeSeparators(file));
            QSettings settings;
            settings.setValue(QStringLiteral("game/javaPath"), file);
            settings.sync();
            emit notifyRequested(QStringLiteral("Java 路径已保存。"));
        }
    });
    gameLayout->addWidget(pathBlock(Icons::Type::Cpu, QStringLiteral("Java 运行时"),
                                    QStringLiteral("留空时自动检测；选择后将用于启动游戏。"),
                                    m_javaEdit, javaBrowse, game));
    gameLayout->addSpacing(12);

    auto *memoryControl = new QWidget(game);
    auto *memoryLayout = new QVBoxLayout(memoryControl);
    memoryLayout->setContentsMargins(0, 0, 0, 0);
    memoryLayout->setSpacing(2);
    auto *memoryValue = Theme::label(QStringLiteral("%1 GB").arg(memoryGb), 11,
                                     QFont::DemiBold, Theme::Accent, memoryControl);
    memoryValue->setAlignment(Qt::AlignRight);
    memoryLayout->addWidget(memoryValue);
    auto *memorySlider = new QSlider(Qt::Horizontal, memoryControl);
    memorySlider->setRange(2, 16);
    memorySlider->setValue(memoryGb);
    memorySlider->setFixedWidth(170);
    connect(memorySlider, &QSlider::valueChanged, this, [memoryValue](int value) {
        memoryValue->setText(QStringLiteral("%1 GB").arg(value));
        QSettings settings;
        settings.setValue(QStringLiteral("game/memoryGb"), value);
    });
    memoryLayout->addWidget(memorySlider);
    gameLayout->addWidget(settingRow(
        Icons::Type::Memory, QStringLiteral("内存分配"),
        QStringLiteral("启动游戏时使用的最大 Java 堆内存，推荐 6–8 GB。"),
        memoryControl, game));
    gameLayout->addSpacing(12);

    m_gameDirEdit = new QLineEdit(
        QDir::toNativeSeparators(MinecraftPaths::savedGameDirectory()), game);
    m_gameDirEdit->setReadOnly(true);
    auto *gameDirBrowse = new AccentButton(QStringLiteral("浏览"), Icons::Type::Folder, game);
    connect(gameDirBrowse, &QAbstractButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择 .minecraft 目录"), m_gameDirEdit->text());
        if (!directory.isEmpty()) {
            const QString absolute = QDir(directory).absolutePath();
            m_gameDirEdit->setText(QDir::toNativeSeparators(absolute));
            MinecraftPaths::saveGameDirectory(absolute);
            emit gameDirectoryChanged(absolute);
        }
    });
    gameLayout->addWidget(pathBlock(Icons::Type::Folder, QStringLiteral("游戏目录"),
                                    QStringLiteral("本地版本、模组与存档所在的根目录。"),
                                    m_gameDirEdit, gameDirBrowse, game));
    gameLayout->addSpacing(12);

    auto *sourceCombo = new QComboBox(game);
    sourceCombo->setFixedWidth(220);
    sourceCombo->addItem(QStringLiteral("BMCLAPI 镜像（国内推荐）"),
                         QStringLiteral("bmclapi"));
    sourceCombo->addItem(QStringLiteral("Mojang 官方源"),
                         QStringLiteral("official"));
    const int sourceIndex = sourceCombo->findData(
        downloadSource == QStringLiteral("official")
            ? QStringLiteral("official")
            : QStringLiteral("bmclapi"));
    sourceCombo->setCurrentIndex(qMax(0, sourceIndex));
    connect(sourceCombo, &QComboBox::currentIndexChanged, this,
            [this, sourceCombo](int) {
                const QString source = sourceCombo->currentData().toString();
                QSettings settings;
                settings.setValue(QStringLiteral("network/downloadSource"), source);
                settings.sync();
                MojangMirror::setDownloadSource(
                    source == QStringLiteral("official")
                        ? MojangMirror::DownloadSource::Official
                        : MojangMirror::DownloadSource::Bmclapi);
                emit notifyRequested(
                    source == QStringLiteral("official")
                        ? QStringLiteral("已切换到 Mojang 官方下载源。")
                        : QStringLiteral("已切换到 BMCLAPI 镜像下载源。"));
            });
    gameLayout->addWidget(settingRow(
        Icons::Type::Download, QStringLiteral("游戏文件下载源"),
        QStringLiteral("影响版本清单、客户端、依赖库和资源文件；加载器使用各自接口。"),
        sourceCombo, game));
    gameLayout->addStretch();

    cardsRow->addWidget(appearance, 1);
    cardsRow->addWidget(game, 1);
    contentLayout->addLayout(cardsRow);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void SettingsPage::setGameDirectory(const QString &directory)
{
    if (!m_gameDirEdit || directory.trimmed().isEmpty()) {
        return;
    }
    m_gameDirEdit->setText(QDir::toNativeSeparators(QDir(directory).absolutePath()));
}

void SettingsPage::setJavaPath(const QString &path)
{
    if (!m_javaEdit || path.trimmed().isEmpty()) {
        return;
    }
    m_javaEdit->setText(QDir::toNativeSeparators(path));
}
