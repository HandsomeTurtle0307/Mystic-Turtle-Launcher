/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include "DownloadCenterPage.h"

#include "DownloadGamePage.h"
#include "DownloadModsPage.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QPushButton *categoryButton(const QString &title, Icons::Type icon, QWidget *parent)
{
    auto *button = new QPushButton(parent);
    button->setText(title);
    button->setIcon(Icons::icon(icon, Theme::TextMuted, 19));
    button->setIconSize(QSize(17, 17));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(36);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; spacing: 8px; padding: 0 10px;"
        "border: 1px solid transparent; border-radius: 8px;"
        "background: transparent; color: %1; font-size: 12px; font-weight: 500; }"
        "QPushButton:hover { background: rgba(255,255,255,12); color: %2; }"
        "QPushButton:checked { background: rgba(137,168,255,30);"
        "border-color: rgba(137,168,255,50); color: #B8CCFF; font-weight: 600; }")
        .arg(Theme::TextSecondary.name(), Theme::TextPrimary.name()));
    return button;
}

} // namespace

DownloadCenterPage::DownloadCenterPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    auto *sidebar = new QWidget(this);
    sidebar->setAttribute(Qt::WA_NoSystemBackground);
    sidebar->setAutoFillBackground(false);
    sidebar->setFixedWidth(164);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 2, 0, 2);
    sidebarLayout->setSpacing(3);

    m_content = new QStackedWidget(this);
    m_content->setFrameShape(QFrame::NoFrame);
    m_content->setAttribute(Qt::WA_StyledBackground, false);
    m_content->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));

    m_gamePage = new DownloadGamePage(m_content);
    m_modsPage = new DownloadModsPage(QStringLiteral("mod"), QStringLiteral("Mod"),
                                      m_content);
    auto *modpackPage = new DownloadModsPage(QStringLiteral("modpack"),
                                              QStringLiteral("整合包"), m_content);
    auto *datapackPage = new DownloadModsPage(QStringLiteral("datapack"),
                                              QStringLiteral("数据包"), m_content);
    auto *resourcePackPage = new DownloadModsPage(QStringLiteral("resourcepack"),
                                                   QStringLiteral("资源包"), m_content);
    auto *shaderPage = new DownloadModsPage(QStringLiteral("shader"),
                                             QStringLiteral("光影包"), m_content);
    m_content->addWidget(m_gamePage);
    m_content->addWidget(m_modsPage);
    m_content->addWidget(modpackPage);
    m_content->addWidget(datapackPage);
    m_content->addWidget(resourcePackPage);
    m_content->addWidget(shaderPage);
    m_projectPages = {m_modsPage, modpackPage, datapackPage,
                      resourcePackPage, shaderPage};
    for (DownloadModsPage *page : m_projectPages) {
        connectProjectPage(page);
    }

    auto *group = new QButtonGroup(sidebar);
    group->setExclusive(true);
    const struct {
        const char *title;
        Icons::Type icon;
    } categories[] = {
        {"原版游戏", Icons::Type::Cube},
        {"Mod", Icons::Type::Puzzle},
        {"整合包", Icons::Type::Boxes},
        {"数据包", Icons::Type::Grid},
        {"资源包", Icons::Type::Image},
        {"光影包", Icons::Type::Sun},
    };

    auto *original = categoryButton(QString::fromUtf8(categories[0].title),
                                    categories[0].icon, sidebar);
    original->setIcon(Icons::icon(categories[0].icon, Theme::Blue, 17));
    original->setChecked(true);
    group->addButton(original, 0);
    sidebarLayout->addWidget(original);
    connect(original, &QPushButton::clicked, this, [this]() {
        m_content->setCurrentIndex(0);
    });

    auto *communityTitle = Theme::label(QStringLiteral("社区资源"), 10, QFont::DemiBold,
                                        Theme::TextMuted, sidebar);
    communityTitle->setContentsMargins(9, 8, 0, 3);
    sidebarLayout->addWidget(communityTitle);
    sidebarLayout->addWidget(Theme::divider(sidebar));
    sidebarLayout->addSpacing(2);

    for (int index = 1; index < 6; ++index) {
        auto *button = categoryButton(QString::fromUtf8(categories[index].title),
                                      categories[index].icon, sidebar);
        group->addButton(button, index);
        sidebarLayout->addWidget(button);
        connect(button, &QPushButton::toggled, this,
                [button, icon = categories[index].icon](bool checked) {
            button->setIcon(Icons::icon(icon, checked ? Theme::Blue : Theme::TextMuted, 17));
        });
        connect(button, &QPushButton::clicked, this, [this, index]() {
            m_content->setCurrentIndex(index);
        });
    }
    sidebarLayout->addStretch();

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet(QStringLiteral(
        "QFrame { color: rgba(255,255,255,24); max-width: 1px; }"));

    root->addWidget(sidebar);
    root->addWidget(separator);
    root->addWidget(m_content, 1);
}

DownloadGamePage *DownloadCenterPage::gamePage() const
{
    return m_gamePage;
}

DownloadModsPage *DownloadCenterPage::modsPage() const
{
    return m_modsPage;
}

void DownloadCenterPage::setGameDirectory(const QString &directory)
{
    m_gamePage->setGameDirectory(directory);
    for (DownloadModsPage *page : m_projectPages) {
        page->setGameDirectory(directory);
    }
}

void DownloadCenterPage::connectProjectPage(DownloadModsPage *page)
{
    connect(page, &DownloadModsPage::notifyRequested,
            this, &DownloadCenterPage::notifyRequested);
    connect(page, &DownloadModsPage::modsChanged,
            this, &DownloadCenterPage::modsChanged);
}
