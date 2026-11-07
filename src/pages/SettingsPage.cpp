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
#include "GlobalSettingsWidget.h"
#include "IconFactory.h"
#include "MinecraftPaths.h"
#include "Theme.h"
#include "ToggleSwitch.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QImageReader>
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
    if (!description.isEmpty()) {
        text->addWidget(Theme::label(description, 10, QFont::Normal, Theme::TextMuted, row));
    }
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
    const QString backgroundImage =
        initialSettings.value(QStringLiteral("ui/backgroundImage")).toString();
    m_backgroundOpacity = qBound(
        0, initialSettings.value(QStringLiteral("ui/backgroundOpacity"), 35).toInt(), 100);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 22, 32, 16);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);
    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(3);
    headerText->addWidget(Theme::label(QStringLiteral("设置"), 27, QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    headerText->addWidget(Theme::label(QStringLiteral("调整启动、内存、皮肤与高级运行选项。"),
                                       12, QFont::Normal, Theme::TextMuted, this));
    header->addLayout(headerText);
    header->addStretch();
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

    m_globalSettings = new GlobalSettingsWidget(content);
    connect(m_globalSettings, &GlobalSettingsWidget::notifyRequested,
            this, &SettingsPage::notifyRequested);
    connect(m_globalSettings, &GlobalSettingsWidget::versionIsolationChanged,
            this, &SettingsPage::versionIsolationChanged);

    auto *cardsRow = new QHBoxLayout();
    cardsRow->setContentsMargins(0, 0, 0, 0);
    cardsRow->setSpacing(14);

    // Appearance
    auto *appearance = new CardWidget(content);
    appearance->setAccentColor(Theme::Accent);
    appearance->setMinimumHeight(450);
    appearance->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *appearanceLayout = new QVBoxLayout(appearance);
    appearanceLayout->setContentsMargins(22, 19, 22, 19);
    appearanceLayout->setSpacing(0);
    appearanceLayout->addWidget(Theme::label(QStringLiteral("外观"), 17, QFont::DemiBold,
                                             Theme::TextPrimary, appearance));
    appearanceLayout->addWidget(Theme::label(QStringLiteral("界面材质、背景与过渡动效。"),
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
        QStringLiteral("控制平滑滚动、按钮点击、页面切换、进度条、下拉菜单及其他界面动效。"),
        animationsSwitch, appearance));
    appearanceLayout->addSpacing(11);

    m_backgroundPathEdit = new QLineEdit(appearance);
    m_backgroundPathEdit->setReadOnly(true);
    m_backgroundPathEdit->setPlaceholderText(QStringLiteral("未设置背景图片"));
    if (!backgroundImage.isEmpty()) {
        m_backgroundPathEdit->setText(QDir::toNativeSeparators(backgroundImage));
    }

    auto *backgroundBrowse = new AccentButton(
        QStringLiteral("选择"), Icons::Type::Palette, appearance);
    backgroundBrowse->setCompact(true);
    backgroundBrowse->setVariant(AccentButton::Variant::Ghost);
    connect(backgroundBrowse, &QAbstractButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择背景图片"), QString(),
            QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
        if (file.isEmpty()) {
            return;
        }

        QImageReader reader(file);
        reader.setAutoTransform(true);
        if (reader.read().isNull()) {
            emit notifyRequested(QStringLiteral("无法读取所选图片：%1").arg(reader.errorString()));
            return;
        }
        setBackgroundImage(file);
        emit notifyRequested(QStringLiteral("背景图片已更新。"));
    });

    auto *backgroundClear = new AccentButton(
        QStringLiteral("清除"), Icons::Type::Close, appearance);
    backgroundClear->setCompact(true);
    backgroundClear->setVariant(AccentButton::Variant::Ghost);
    connect(backgroundClear, &QAbstractButton::clicked, this, [this]() {
        setBackgroundImage(QString());
        emit notifyRequested(QStringLiteral("已清除自定义背景图片。"));
    });

    auto *backgroundBlock = new QWidget(appearance);
    auto *backgroundLayout = new QVBoxLayout(backgroundBlock);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->setSpacing(7);
    backgroundLayout->addWidget(settingRow(
        Icons::Type::Palette, QStringLiteral("自定义背景图片"),
        QStringLiteral("选择本地图片作为启动器界面背景。"),
        nullptr, backgroundBlock));
    auto *backgroundPathRow = new QHBoxLayout();
    backgroundPathRow->setContentsMargins(0, 0, 0, 0);
    backgroundPathRow->setSpacing(8);
    m_backgroundPathEdit->setFixedHeight(38);
    m_backgroundPathEdit->setFont(Theme::font(11));
    backgroundPathRow->addWidget(m_backgroundPathEdit, 1);
    backgroundPathRow->addWidget(backgroundBrowse);
    backgroundPathRow->addWidget(backgroundClear);
    backgroundLayout->addLayout(backgroundPathRow);
    appearanceLayout->addWidget(backgroundBlock);
    appearanceLayout->addSpacing(9);

    auto *opacityControl = new QWidget(appearance);
    auto *opacityLayout = new QVBoxLayout(opacityControl);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(2);
    auto *opacityValue = Theme::label(
        QStringLiteral("%1%").arg(m_backgroundOpacity), 11,
        QFont::DemiBold, Theme::Accent, opacityControl);
    opacityValue->setAlignment(Qt::AlignRight);
    opacityLayout->addWidget(opacityValue);
    auto *opacitySlider = new QSlider(Qt::Horizontal, opacityControl);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(m_backgroundOpacity);
    opacitySlider->setFixedWidth(170);
    opacityLayout->addWidget(opacitySlider);
    connect(opacitySlider, &QSlider::valueChanged, this, [this, opacityValue](int value) {
        m_backgroundOpacity = value;
        opacityValue->setText(QStringLiteral("%1%").arg(m_backgroundOpacity));
        QSettings settings;
        settings.setValue(QStringLiteral("ui/backgroundOpacity"), m_backgroundOpacity);
        emit backgroundChanged(m_backgroundPathEdit->text(), m_backgroundOpacity);
    });
    appearanceLayout->addWidget(settingRow(
        Icons::Type::Sparkles, QStringLiteral("背景图片不透明度"),
        QStringLiteral("0% 为完全透明，100% 为完全不透明。"),
        opacityControl, appearance));

    appearanceLayout->addStretch();

    // Download and local files
    auto *game = new CardWidget(content);
    game->setAccentColor(Theme::Blue);
    game->setMinimumHeight(260);
    game->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *gameLayout = new QVBoxLayout(game);
    gameLayout->setContentsMargins(22, 19, 22, 19);
    gameLayout->setSpacing(0);
    gameLayout->addWidget(Theme::label(QStringLiteral("下载与本地文件"), 17, QFont::DemiBold,
                                       Theme::TextPrimary, game));
    gameLayout->addWidget(Theme::label(QStringLiteral("游戏目录与文件下载源。"),
                                       10, QFont::Normal, Theme::TextMuted, game));
    gameLayout->addSpacing(12);
    gameLayout->addWidget(Theme::divider(game));
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

    gameLayout->addStretch();

    cardsRow->addWidget(appearance, 1);
    cardsRow->addWidget(game, 1);
    contentLayout->addLayout(cardsRow);
    contentLayout->addWidget(m_globalSettings);
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
    if (m_globalSettings) {
        m_globalSettings->setGameDirectory(directory);
    }
}

void SettingsPage::setJavaPath(const QString &path)
{
    if (!m_globalSettings || path.trimmed().isEmpty()) {
        return;
    }
    m_globalSettings->setJavaPath(path);
}

void SettingsPage::flushGlobalSettings()
{
    if (m_globalSettings) {
        m_globalSettings->flush();
    }
}

void SettingsPage::setBackgroundImage(const QString &imagePath)
{
    const QString absolutePath = imagePath.isEmpty()
        ? QString()
        : QDir::toNativeSeparators(QFileInfo(imagePath).absoluteFilePath());
    m_backgroundPathEdit->setText(absolutePath);

    QSettings settings;
    if (absolutePath.isEmpty()) {
        settings.remove(QStringLiteral("ui/backgroundImage"));
    } else {
        settings.setValue(QStringLiteral("ui/backgroundImage"), absolutePath);
    }
    settings.sync();
    emit backgroundChanged(absolutePath, m_backgroundOpacity);
}
