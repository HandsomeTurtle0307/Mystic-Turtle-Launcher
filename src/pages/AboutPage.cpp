/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AboutPage.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "LogoMark.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QWidget *infoRow(const QString &label, const QString &value, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    auto *key = Theme::label(label, 11, QFont::DemiBold, Theme::TextMuted, row);
    key->setFixedWidth(84);
    layout->addWidget(key);
    auto *text = Theme::label(value, 12, QFont::Normal, Theme::TextPrimary, row);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(text, 1);
    return row;
}

QToolButton *creditLink(const QString &name,
                        Icons::Type icon,
                        const QColor &accent,
                        const QString &url,
                        QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(name);
    button->setIcon(Icons::icon(icon, accent, 20));
    button->setIconSize(QSize(20, 20));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; border: 1px solid %2; border-radius: 9px;"
        "padding: 9px 12px; text-align: left; }"
        "QToolButton:hover { background: rgba(%3, %4, %5, 24); }")
        .arg(Theme::TextPrimary.name(), Theme::Border.name())
        .arg(accent.red())
        .arg(accent.green())
        .arg(accent.blue()));
    button->setToolTip(QStringLiteral("打开 %1 官网").arg(name));
    QObject::connect(button, &QToolButton::clicked, button, [url]() {
        QDesktopServices::openUrl(QUrl(url));
    });
    return button;
}

} // namespace

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(34, 26, 34, 24);
    root->setSpacing(0);

    auto *warning = new QLabel(this);
    warning->setTextFormat(Qt::RichText);
    warning->setWordWrap(true);
    warning->setOpenExternalLinks(true);
    warning->setText(QStringLiteral(
        "由于当前版本功能过多，作者一个人有点测不太过来了...不过我尽量排查了一遍。"
        "如果你在使用过程中发现了任何问题，欢迎在"
        "<a href=\"https://github.com/HandsomeTurtle0307/Mystic-Turtle-Launcher/issues\">"
        "Issues</a>里分享！谢谢大家啦！OvO"));
    warning->setStyleSheet(QStringLiteral(
        "QLabel { color: #F5B45C; background: rgba(245, 158, 66, 22);"
        "border: 1px solid rgba(245, 158, 66, 95); border-radius: 9px;"
        "padding: 10px 13px; }"
        "QLabel a { color: #FFC477; font-weight: 600; }"));
    root->addWidget(warning);
    root->addSpacing(14);

    root->addWidget(Theme::label(QStringLiteral("关于 MysticTurtle"), 27, QFont::DemiBold,
                                 Theme::TextPrimary, this, true));
    root->addWidget(Theme::label(QStringLiteral("呜呜呜，可是乌龟真的很可爱啊ovo"),
                                 11, QFont::Normal, Theme::TextMuted, this));
    root->addSpacing(22);

    auto *cardRow = new QHBoxLayout();
    cardRow->setContentsMargins(0, 0, 0, 0);
    cardRow->addStretch();

    auto *card = new CardWidget(this);
    card->setAccentColor(Theme::Accent);
    card->setInteractive(false);
    card->setMinimumWidth(620);
    card->setMaximumWidth(760);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(30, 26, 30, 26);
    cardLayout->setSpacing(0);

    auto *head = new QHBoxLayout();
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(18);
    head->addWidget(new LogoMark(64, card), 0, Qt::AlignVCenter);

    auto *headText = new QVBoxLayout();
    headText->setContentsMargins(0, 0, 0, 0);
    headText->setSpacing(2);
    headText->addWidget(Theme::label(QStringLiteral("MysticTurtle Launcher"),
                                     20, QFont::DemiBold, Theme::TextPrimary, card, true));
    head->addLayout(headText, 1);
    cardLayout->addLayout(head);
    cardLayout->addSpacing(20);
    cardLayout->addWidget(Theme::divider(card));
    cardLayout->addSpacing(16);

    cardLayout->addWidget(infoRow(QStringLiteral("作者"),
                                  QStringLiteral("HandsomeTurtle0307"), card));
    cardLayout->addSpacing(12);
    cardLayout->addWidget(infoRow(QStringLiteral("当前版本"),
                                  QStringLiteral("v2.0"), card));
    cardLayout->addSpacing(12);
    cardLayout->addWidget(Theme::label(QStringLiteral("鸣谢"), 11, QFont::DemiBold,
                                       Theme::TextMuted, card));
    cardLayout->addSpacing(10);
    auto *credits = new QGridLayout();
    credits->setContentsMargins(0, 0, 0, 0);
    credits->setHorizontalSpacing(10);
    credits->setVerticalSpacing(8);
    credits->addWidget(creditLink(QStringLiteral("Qt"), Icons::Type::Package,
                                  Theme::AccentStrong, QStringLiteral("https://www.qt.io/"),
                                  card), 0, 0);
    credits->addWidget(creditLink(QStringLiteral("DeepSeek Harness"),
                                  Icons::Type::Sparkles, Theme::Purple,
                                  QStringLiteral("https://www.deepseek.com/"), card),
                       0, 1);
    credits->addWidget(creditLink(QStringLiteral("Visual Studio Code"),
                                  Icons::Type::Cpu, Theme::Blue,
                                  QStringLiteral("https://code.visualstudio.com/"), card),
                       1, 0);
    credits->addWidget(creditLink(QStringLiteral("GitHub Copilot"),
                                  Icons::Type::External, Theme::Accent,
                                  QStringLiteral("https://github.com/features/copilot"), card),
                       1, 1);
    cardLayout->addLayout(credits);
    cardLayout->addSpacing(18);
    cardLayout->addWidget(Theme::divider(card));
    cardLayout->addSpacing(16);

    auto *repoRow = new QHBoxLayout();
    repoRow->setContentsMargins(0, 0, 0, 0);
    repoRow->setSpacing(12);
    auto *repoText = new QVBoxLayout();
    repoText->setContentsMargins(0, 0, 0, 0);
    repoText->setSpacing(2);
    repoText->addWidget(Theme::label(QStringLiteral("项目仓库"), 11, QFont::DemiBold,
                                     Theme::TextPrimary, card));
    repoText->addWidget(Theme::label(
        QStringLiteral("https://github.com/HandsomeTurtle0307/Mystic-Turtle-Launcher"),
        10, QFont::Normal, Theme::TextMuted, card));
    repoRow->addLayout(repoText, 1);

    auto *repoButton = new AccentButton(QStringLiteral("打开 GitHub"), Icons::Type::External,
                                        card);
    repoButton->setVariant(AccentButton::Variant::Primary);
    repoButton->setCompact(false);
    connect(repoButton, &QAbstractButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(
            QStringLiteral("https://github.com/HandsomeTurtle0307/Mystic-Turtle-Launcher")));
    });
    repoRow->addWidget(repoButton, 0, Qt::AlignVCenter);
    cardLayout->addLayout(repoRow);

    cardRow->addWidget(card);
    cardRow->addStretch();
    root->addLayout(cardRow);
    root->addStretch(1);
}
