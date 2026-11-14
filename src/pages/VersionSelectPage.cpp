/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionSelectPage.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "Theme.h"
#include "VersionCard.h"

#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

VersionSelectPage::VersionSelectPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(34, 24, 34, 24);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(14);

    auto *back = new AccentButton(QStringLiteral("返回主页"),
                                  Icons::Type::ChevronLeft, this);
    back->setVariant(AccentButton::Variant::Secondary);
    back->setCompact(true);
    connect(back, &QAbstractButton::clicked, this, &VersionSelectPage::backRequested);
    header->addWidget(back, 0, Qt::AlignVCenter);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(2);
    titleBlock->addWidget(Theme::label(QStringLiteral("选择游戏版本"),
                                       23, QFont::DemiBold, Theme::TextPrimary,
                                       this, true));
    m_countLabel = Theme::label(QString(), 10, QFont::Normal,
                                Theme::TextMuted, this);
    titleBlock->addWidget(m_countLabel);
    header->addLayout(titleBlock, 1);
    root->addLayout(header);
    root->addSpacing(18);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);

    m_cardsContent = new QWidget();
    m_cardsContent->setAttribute(Qt::WA_NoSystemBackground);
    auto *contentLayout = new QVBoxLayout(m_cardsContent);
    contentLayout->setContentsMargins(0, 0, 8, 4);
    contentLayout->setSpacing(0);

    m_grid = new QGridLayout();
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(14);
    m_grid->setVerticalSpacing(14);
    contentLayout->addLayout(m_grid);

    m_emptyLabel = Theme::label(QStringLiteral("当前目录下没有已安装版本。"),
                                12, QFont::Normal, Theme::TextMuted,
                                m_cardsContent);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setMinimumHeight(160);
    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addStretch(1);

    m_scroll->setWidget(m_cardsContent);
    root->addWidget(m_scroll, 1);

    rebuildCards();
}

void VersionSelectPage::setVersions(const QVector<VersionInfo> &versions,
                                    int selectedIndex,
                                    const QString &gameDirectory)
{
    m_versions = versions;
    m_selectedIndex = selectedIndex;
    m_gameDirectory = gameDirectory;
    if (m_countLabel) {
        m_countLabel->setText(QStringLiteral("%1 个已安装版本").arg(m_versions.size()));
        m_countLabel->setToolTip(QDir::toNativeSeparators(m_gameDirectory));
    }
    rebuildCards();
}

void VersionSelectPage::setSelectedIndex(int selectedIndex)
{
    m_selectedIndex = selectedIndex;
    for (int i = 0; i < m_cards.size(); ++i) {
        m_cards.at(i)->setSelected(i == selectedIndex);
    }
}

void VersionSelectPage::rebuildCards()
{
    if (!m_grid) {
        return;
    }

    m_cards.clear();
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    for (int i = 0; i < m_versions.size(); ++i) {
        auto *card = new VersionCard(m_versions.at(i), i, m_cardsContent);
        card->setSelected(i == m_selectedIndex);
        connect(card, &VersionCard::selected, this,
                &VersionSelectPage::versionSelected);
        connect(card, &VersionCard::detailsRequested, this,
                &VersionSelectPage::detailsRequested);
        connect(card, &VersionCard::deleteRequested, this,
                &VersionSelectPage::deleteRequested);
        m_cards.append(card);
        m_grid->addWidget(card, i / 3, i % 3);
    }

    for (int column = 0; column < 3; ++column) {
        m_grid->setColumnStretch(column, 1);
    }
    if (m_emptyLabel) {
        m_emptyLabel->setVisible(m_versions.isEmpty());
    }
}
