/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "MorePage.h"

#include "AccentButton.h"
#include "AboutPage.h"
#include "AnimatedStackedWidget.h"
#include "CardWidget.h"
#include "DonationPage.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <functional>

namespace {

QWidget *wrapSubpage(const QString &title, QWidget *page,
                     const std::function<void()> &goBack,
                     QWidget *parent)
{
    auto *wrapper = new QWidget(parent);
    wrapper->setAttribute(Qt::WA_NoSystemBackground);
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(24, 10, 24, 0);
    auto *back = new AccentButton(QStringLiteral("返回更多"),
                                  Icons::Type::ChevronLeft, wrapper);
    back->setVariant(AccentButton::Variant::Secondary);
    back->setCompact(true);
    QObject::connect(back, &QAbstractButton::clicked, wrapper,
                     [goBack]() { goBack(); });
    header->addWidget(back, 0, Qt::AlignLeft);
    header->addStretch();
    header->addWidget(Theme::label(title, 12, QFont::DemiBold,
                                   Theme::TextSecondary, wrapper));
    layout->addLayout(header);
    layout->addWidget(page, 1);
    return wrapper;
}

QWidget *moreTile(const QString &eyebrow, const QString &title,
                  const QString &description, const QString &meta,
                  Icons::Type icon, const QColor &accent,
                  const std::function<void()> &activated, QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(accent);
    card->setInteractive(true);
    card->setMinimumHeight(236);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(0);

    auto *heading = new QHBoxLayout();
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(13);
    auto *iconLabel = new QLabel(card);
    iconLabel->setPixmap(Icons::icon(icon, accent, 26).pixmap(26, 26));
    iconLabel->setFixedSize(46, 46);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(QStringLiteral(
        "background: rgba(%1, %2, %3, 26);"
        "border: 1px solid rgba(%1, %2, %3, 42); border-radius: 14px;")
        .arg(accent.red()).arg(accent.green()).arg(accent.blue()));
    heading->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(2);
    auto *eyebrowLabel = Theme::label(eyebrow, 8, QFont::Bold,
                                      accent, card);
    QFont eyebrowFont = eyebrowLabel->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    eyebrowLabel->setFont(eyebrowFont);
    titleBlock->addWidget(eyebrowLabel);
    titleBlock->addWidget(Theme::label(title, 17, QFont::DemiBold,
                                      Theme::TextPrimary, card));
    heading->addLayout(titleBlock, 1);

    auto *watermark = new QLabel(card);
    QColor watermarkColor = accent;
    watermarkColor.setAlpha(30);
    watermark->setPixmap(Icons::icon(icon, watermarkColor, 66).pixmap(66, 66));
    watermark->setFixedSize(66, 66);
    watermark->setAlignment(Qt::AlignCenter);
    watermark->setAttribute(Qt::WA_TransparentForMouseEvents);
    heading->addWidget(watermark, 0, Qt::AlignTop);
    layout->addLayout(heading);
    layout->addSpacing(13);

    auto *summary = Theme::label(description, 11, QFont::Normal,
                                 Theme::TextMuted, card);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    layout->addSpacing(11);

    if (!meta.trimmed().isEmpty()) {
        layout->addWidget(Theme::badge(
            meta, accent, QColor(accent.red(), accent.green(), accent.blue(), 20),
            card, QColor(accent.red(), accent.green(), accent.blue(), 52)),
            0, Qt::AlignLeft);
    }
    layout->addStretch(1);
    layout->addSpacing(12);
    layout->addWidget(Theme::divider(card));
    layout->addSpacing(12);

    auto *open = new AccentButton(QStringLiteral("打开"), Icons::Type::ChevronRight, card);
    open->setVariant(AccentButton::Variant::Ghost);
    open->setCompact(false);
    open->setMinimumHeight(38);
    open->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(open, &QAbstractButton::clicked, card,
                     [activated]() { activated(); });
    layout->addWidget(open);
    return card;
}

} // namespace

MorePage::MorePage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    m_pages = new AnimatedStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_pages);

    auto *overview = new QWidget(m_pages);
    overview->setAttribute(Qt::WA_NoSystemBackground);
    auto *overviewLayout = new QVBoxLayout(overview);
    overviewLayout->setContentsMargins(34, 28, 34, 28);
    overviewLayout->setSpacing(0);
    overviewLayout->addWidget(Theme::label(QStringLiteral("更多"), 27,
                                           QFont::DemiBold, Theme::TextPrimary,
                                           overview, true));
    overviewLayout->addWidget(Theme::label(
        QStringLiteral("项目相关信息与支持方式"), 11, QFont::Normal,
        Theme::TextMuted, overview));
    overviewLayout->addSpacing(22);

    auto *tiles = new QGridLayout();
    tiles->setContentsMargins(0, 0, 0, 0);
    tiles->setHorizontalSpacing(14);
    tiles->setVerticalSpacing(14);

    auto *about = new AboutPage(m_pages);
    auto *donation = new DonationPage(m_pages);
    auto *aboutWrapper = wrapSubpage(
        QStringLiteral("关于"), about,
        [this]() { showOverview(); }, m_pages);
    auto *donationWrapper = wrapSubpage(
        QStringLiteral("支持项目"), donation,
        [this]() { showOverview(); }, m_pages);

    tiles->addWidget(moreTile(
        QStringLiteral("PROJECT / 项目"),
        QStringLiteral("关于 MysticTurtle"),
        QStringLiteral("查看项目介绍、作者信息、版本信息和鸣谢。"),
        QStringLiteral("作者 · 开源许可"),
        Icons::Type::Info, Theme::Blue,
        [this]() { m_pages->setCurrentIndex(1, true); }, overview), 0, 0);
    tiles->addWidget(moreTile(
        QStringLiteral("SUPPORT / 支持"),
        QStringLiteral("支持项目"),
        QStringLiteral("了解赞赏与更新说明，并查看捐赠二维码。"),
        QStringLiteral("赞赏 · 更新说明"),
        Icons::Type::Sparkles, Theme::Accent,
        [this]() { m_pages->setCurrentIndex(2, true); }, overview), 0, 1);
    tiles->setColumnStretch(0, 1);
    tiles->setColumnStretch(1, 1);
    overviewLayout->addLayout(tiles);
    overviewLayout->addStretch(1);

    m_pages->addPage(overview);
    m_pages->addPage(aboutWrapper);
    m_pages->addPage(donationWrapper);
    m_pages->setCurrentIndex(0, false);
    setAnimationsEnabled(m_animationsEnabled);
}

void MorePage::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (m_pages) {
        m_pages->setAnimationsEnabled(enabled);
    }
    if (auto *donation = findChild<DonationPage *>()) {
        donation->setAnimationsEnabled(enabled);
    }
}

void MorePage::showOverview()
{
    if (m_pages) {
        m_pages->setCurrentIndex(0, m_animationsEnabled);
    }
}
