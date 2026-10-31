/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DownloadModsPage.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "SampleData.h"
#include "SearchField.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

namespace {

class ModTile : public QWidget
{
public:
    ModTile(const QString &name, const QColor &accent, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_letter(name.left(1).toUpper())
        , m_accent(accent)
    {
        setFixedSize(44, 44);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF tile = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        QLinearGradient fill(tile.topLeft(), tile.bottomRight());
        fill.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 72));
        fill.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 18));
        painter.setPen(QPen(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 60), 1.0));
        painter.setBrush(fill);
        painter.drawRoundedRect(tile, 12, 12);

        painter.setPen(QColor(QStringLiteral("#F2F8F5")));
        painter.setFont(Theme::font(19, QFont::DemiBold, true));
        painter.drawText(tile, Qt::AlignCenter, m_letter);
    }

private:
    QString m_letter;
    QColor m_accent;
};

class FeaturedModCard : public CardWidget
{
public:
    explicit FeaturedModCard(QWidget *parent = nullptr)
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

        const QPointF center(width() - 125.0, height() / 2.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                   Theme::Accent.blue(), 24), 1.0));
        painter.drawEllipse(center, 88, 88);
        painter.setPen(QPen(QColor(47, 128, 237, 36), 1.0));
        painter.drawEllipse(center, 118, 118);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                Theme::Accent.blue(), 32));
        painter.drawRoundedRect(QRectF(width() - 250, 30, 64, 64), 16, 16);
        painter.setBrush(QColor(47, 128, 237, 22));
        painter.drawRoundedRect(QRectF(width() - 176, 66, 38, 38), 10, 10);

        painter.setPen(QColor(47, 128, 237, 28));
        painter.setFont(Theme::font(64, QFont::Bold, true));
        painter.drawText(QRectF(width() - 330, height() - 78, 300, 70),
                         Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("SODIUM"));
    }
};

} // namespace

DownloadModsPage::DownloadModsPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 22, 32, 16);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);
    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(3);
    auto *eyebrow = Theme::label(QStringLiteral("模组库  /  MOD CATALOG"), 9, QFont::Bold,
                                 Theme::Accent, this);
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    eyebrow->setFont(eyebrowFont);
    headerText->addWidget(eyebrow);
    headerText->addWidget(Theme::label(QStringLiteral("下载模组"), 27, QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    headerText->addWidget(Theme::label(QStringLiteral("为你的世界加上一点新的魔法。"),
                                       12, QFont::Normal, Theme::TextMuted, this));
    header->addLayout(headerText);
    header->addStretch();
    header->addWidget(Theme::badge(QStringLiteral("UI PREVIEW  ·  下载服务未接入"),
                                   Theme::Blue, QColor(Theme::Blue.red(), Theme::Blue.green(),
                                                       Theme::Blue.blue(), 18),
                                   this, QColor(Theme::Blue.red(), Theme::Blue.green(),
                                                Theme::Blue.blue(), 45)),
                      0, Qt::AlignVCenter);
    root->addLayout(header);
    root->addSpacing(16);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(8);
    m_search = new SearchField(QStringLiteral("搜索模组名称或作者"), this);
    m_search->setMaximumWidth(330);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { refreshCards(); });
    toolbar->addWidget(m_search);
    toolbar->addSpacing(4);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    const QStringList categories = {
        QStringLiteral("全部"),
        QStringLiteral("性能"),
        QStringLiteral("光影"),
        QStringLiteral("科技"),
        QStringLiteral("实用"),
        QStringLiteral("核心"),
        QStringLiteral("建造"),
    };
    int categoryIndex = 0;
    for (const QString &category : categories) {
        auto *chip = new AccentButton(category, this);
        chip->setVariant(AccentButton::Variant::Ghost);
        chip->setCompact(true);
        chip->setCheckable(true);
        chip->setAutoExclusive(false);
        chip->setAccentColor(Theme::Accent);
        if (category == m_category) {
            chip->setChecked(true);
        }
        group->addButton(chip, categoryIndex);
        toolbar->addWidget(chip);
        ++categoryIndex;
    }
    connect(group, &QButtonGroup::idClicked, this, [this, categories](int id) {
        if (id < 0 || id >= categories.size()) {
            return;
        }
        m_category = categories.at(id);
        refreshCards();
    });
    toolbar->addStretch();
    root->addLayout(toolbar);
    root->addSpacing(14);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget();
    content->setAttribute(Qt::WA_NoSystemBackground);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 7, 4);
    contentLayout->setSpacing(0);

    const auto &mods = SampleData::catalogMods();
    auto *featured = new FeaturedModCard(content);
    featured->setFixedHeight(148);
    auto *featuredLayout = new QHBoxLayout(featured);
    featuredLayout->setContentsMargins(26, 18, 26, 18);
    featuredLayout->setSpacing(18);

    auto *featuredText = new QVBoxLayout();
    featuredText->setContentsMargins(0, 0, 0, 0);
    featuredText->setSpacing(2);
    QColor featuredBackground = Theme::Accent;
    featuredBackground.setAlpha(24);
    featuredText->addWidget(Theme::badge(QStringLiteral("本周精选"), Theme::Accent,
                                         featuredBackground, featured,
                                         QColor(Theme::Accent.red(), Theme::Accent.green(),
                                                Theme::Accent.blue(), 65)),
                            0, Qt::AlignLeft);
    featuredText->addSpacing(2);
    featuredText->addWidget(Theme::label(QStringLiteral("Sodium"), 23, QFont::DemiBold,
                                         Theme::TextPrimary, featured, true));
    featuredText->addWidget(Theme::label(QStringLiteral("让每一帧都更顺滑。作者 %1  ·  %2")
                                             .arg(mods.first().author, mods.first().downloads),
                                         11, QFont::Normal, Theme::TextMuted, featured));
    featuredText->addSpacing(5);
    auto *featuredActions = new QHBoxLayout();
    featuredActions->setContentsMargins(0, 0, 0, 0);
    featuredActions->setSpacing(12);
    auto *getFeatured = new AccentButton(QStringLiteral("获取模组"), Icons::Type::Download, featured);
    getFeatured->setVariant(AccentButton::Variant::Primary);
    getFeatured->setMinimumWidth(122);
    connect(getFeatured, &QAbstractButton::clicked, this, [this]() {
        emit notifyRequested(QStringLiteral("获取模组功能将在下一阶段接入，当前仅展示界面。"));
    });
    featuredActions->addWidget(getFeatured);
    featuredActions->addWidget(Theme::label(QStringLiteral("Fabric  ·  1.21.x  ·  客户端"),
                                            10, QFont::Normal, Theme::TextMuted, featured));
    featuredActions->addStretch();
    featuredText->addLayout(featuredActions);
    featuredLayout->addLayout(featuredText, 1);
    featuredLayout->addSpacing(260);
    contentLayout->addWidget(featured);
    contentLayout->addSpacing(18);

    auto *listHeader = new QHBoxLayout();
    listHeader->setContentsMargins(2, 0, 2, 0);
    listHeader->setSpacing(10);
    listHeader->addWidget(Theme::label(QStringLiteral("探索模组"), 16, QFont::DemiBold,
                                       Theme::TextPrimary, content));
    m_countLabel = Theme::label(QString(), 10, QFont::Normal, Theme::TextMuted, content);
    listHeader->addWidget(m_countLabel);
    listHeader->addStretch();
    listHeader->addWidget(Theme::label(QStringLiteral("示例数据 · 仅用于界面预览"), 10,
                                       QFont::Normal, Theme::TextMuted, content));
    contentLayout->addLayout(listHeader);
    contentLayout->addSpacing(10);

    m_grid = new QGridLayout();
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(12);
    m_grid->setVerticalSpacing(12);
    contentLayout->addLayout(m_grid);

    m_emptyLabel = Theme::label(QStringLiteral("没有找到匹配的模组，试试其他分类或关键词。"),
                                12, QFont::Normal, Theme::TextMuted, content);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFixedHeight(110);
    m_emptyLabel->hide();
    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    refreshCards();
}

void DownloadModsPage::refreshCards()
{
    if (!m_grid) {
        return;
    }

    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    const QString query = m_search ? m_search->text().trimmed() : QString();
    const auto &mods = SampleData::catalogMods();
    int shown = 0;

    for (const CatalogMod &mod : mods) {
        if (m_category != QStringLiteral("全部") && mod.category != m_category) {
            continue;
        }
        if (!query.isEmpty()
            && !mod.name.contains(query, Qt::CaseInsensitive)
            && !mod.author.contains(query, Qt::CaseInsensitive)
            && !mod.summary.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        auto *card = new CardWidget();
        card->setAccentColor(mod.accent);
        card->setCornerRadius(16.0);
        card->setFixedHeight(188);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(17, 15, 17, 14);
        layout->setSpacing(0);

        auto *top = new QHBoxLayout();
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(10);
        top->addWidget(new ModTile(mod.name, mod.accent, card), 0, Qt::AlignTop);

        auto *titleBlock = new QVBoxLayout();
        titleBlock->setContentsMargins(0, 1, 0, 0);
        titleBlock->setSpacing(0);
        titleBlock->addWidget(Theme::label(mod.name, 15, QFont::DemiBold,
                                           Theme::TextPrimary, card));
        titleBlock->addWidget(Theme::label(QStringLiteral("by %1").arg(mod.author), 10,
                                           QFont::Normal, Theme::TextMuted, card));
        top->addLayout(titleBlock, 1);

        QColor categoryBackground = mod.accent;
        categoryBackground.setAlpha(18);
        QColor categoryBorder = mod.accent;
        categoryBorder.setAlpha(48);
        top->addWidget(Theme::badge(mod.category, mod.accent, categoryBackground,
                                    card, categoryBorder), 0, Qt::AlignTop);
        layout->addLayout(top);
        layout->addSpacing(10);

        auto *summary = Theme::label(mod.summary, 11, QFont::Normal, Theme::TextSecondary, card);
        summary->setWordWrap(true);
        summary->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        summary->setFixedHeight(32);
        layout->addWidget(summary);
        layout->addStretch();

        auto *bottom = new QHBoxLayout();
        bottom->setContentsMargins(0, 0, 0, 0);
        bottom->setSpacing(6);
        auto *downloadsIcon = new QLabel(card);
        downloadsIcon->setPixmap(Icons::icon(Icons::Type::Download, Theme::TextMuted, 13)
                                     .pixmap(13, 13));
        downloadsIcon->setFixedSize(13, 13);
        downloadsIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
        bottom->addWidget(downloadsIcon);
        bottom->addWidget(Theme::label(mod.downloads, 10, QFont::Normal,
                                       Theme::TextMuted, card));
        bottom->addStretch();

        auto *getButton = new AccentButton(QStringLiteral("获取"), Icons::Type::Download, card);
        getButton->setVariant(AccentButton::Variant::Secondary);
        getButton->setCompact(true);
        getButton->setAccentColor(mod.accent);
        getButton->setAccessibleName(QStringLiteral("获取模组 %1").arg(mod.name));
        connect(getButton, &QAbstractButton::clicked, this, [this, name = mod.name]() {
            emit notifyRequested(QStringLiteral("%1：模组下载逻辑将在下一阶段接入。").arg(name));
        });
        bottom->addWidget(getButton);
        layout->addLayout(bottom);

        m_grid->addWidget(card, shown / 3, shown % 3);
        ++shown;
    }

    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    m_grid->setColumnStretch(2, 1);
    m_countLabel->setText(QStringLiteral("共 %1 个模组").arg(shown));
    m_emptyLabel->setVisible(shown == 0);
}
