/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionCard.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QWidget *metaItem(Icons::Type icon, const QString &text, QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *iconLabel = new QLabel(widget);
    iconLabel->setPixmap(Icons::icon(icon, Theme::TextMuted, 14).pixmap(14, 14));
    iconLabel->setFixedSize(14, 14);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *textLabel = Theme::label(text, 11, QFont::Normal, Theme::TextSecondary, widget);

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel);
    return widget;
}

} // namespace

VersionCard::VersionCard(const VersionInfo &version, int index, QWidget *parent)
    : CardWidget(parent)
    , m_index(index)
    , m_accent(version.accent)
{
    setAccentColor(version.accent);
    setMinimumHeight(160);
    setMaximumHeight(160);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 13, 16, 12);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetNoConstraint);

    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    QColor badgeBackground = version.accent;
    badgeBackground.setAlpha(24);
    QColor badgeBorder = version.accent;
    badgeBorder.setAlpha(70);
    topRow->addWidget(Theme::badge(QStringLiteral("%1 %2").arg(version.loader, version.loaderVersion),
                                   version.accent, badgeBackground, this, badgeBorder));
    topRow->addStretch();

    auto *status = Theme::label(QStringLiteral("●  就绪"), 10, QFont::DemiBold,
                                version.accent, this);
    m_statusLabel = status;
    topRow->addWidget(status);
    layout->addLayout(topRow);
    layout->addSpacing(9);

    const QString displayName = version.displayName.isEmpty()
        ? version.minecraft
        : version.displayName;
    m_versionLabel = Theme::label(displayName, 23, QFont::DemiBold,
                                  Theme::TextPrimary, this, true);
    m_versionLabel->setMinimumWidth(0);
    m_versionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_fullVersionText = displayName;
    layout->addWidget(m_versionLabel);
    layout->addSpacing(6);

    auto *metaRow = new QHBoxLayout();
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(8);
    if (version.mods.isEmpty()) {
        metaRow->addWidget(metaItem(
            Icons::Type::Cube,
            version.loader == QStringLiteral("原版") ? QStringLiteral("原版体验")
                                                     : QStringLiteral("0 个模组"),
            this));
    } else {
        const int enabled = SampleData::enabledModCount(version);
        metaRow->addWidget(metaItem(Icons::Type::Cube,
                                    QStringLiteral("%1 个模组 · %2 已启用")
                                        .arg(version.mods.size())
                                        .arg(enabled),
                                    this));
    }
    metaRow->addStretch();
    layout->addLayout(metaRow);
    layout->addStretch();

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(8);
    bottomRow->addStretch();

    auto *details = new AccentButton(QStringLiteral("查看详情"), this);
    details->setVariant(AccentButton::Variant::Ghost);
    details->setCompact(true);
    details->setAccentColor(version.accent);
    details->setAccessibleName(QStringLiteral("查看 %1 的已安装模组").arg(version.minecraft));
    connect(details, &QAbstractButton::clicked, this, [this]() {
        emit detailsRequested(m_index);
    });
    bottomRow->addWidget(details);

    auto *remove = new AccentButton(QStringLiteral("删除"), this);
    remove->setVariant(AccentButton::Variant::Danger);
    remove->setCompact(true);
    remove->setAccessibleName(QStringLiteral("删除 Minecraft %1 版本").arg(version.minecraft));
    connect(remove, &QAbstractButton::clicked, this, [this]() {
        emit deleteRequested(m_index);
    });
    bottomRow->addWidget(remove);
    layout->addLayout(bottomRow);
}

void VersionCard::setSelected(bool selected)
{
    if (m_selected == selected) {
        return;
    }
    m_selected = selected;
    setAccentColor(selected ? Theme::Accent : m_accent);
    if (m_statusLabel) {
        m_statusLabel->setText(selected ? QStringLiteral("●  已选择")
                                        : QStringLiteral("●  就绪"));
        QPalette palette = m_statusLabel->palette();
        const QColor color = selected ? Theme::Accent : m_accent;
        palette.setColor(QPalette::WindowText, color);
        palette.setColor(QPalette::Text, color);
        m_statusLabel->setPalette(palette);
    }
}

bool VersionCard::isSelected() const
{
    return m_selected;
}

void VersionCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit selected(m_index);
        event->accept();
        return;
    }
    CardWidget::mousePressEvent(event);
}

void VersionCard::resizeEvent(QResizeEvent *event)
{
    CardWidget::resizeEvent(event);
    QTimer::singleShot(0, this, [this]() { updateVersionElide(); });
}

void VersionCard::updateVersionElide()
{
    // The label width is only reliable after the grid has performed layout.
    if (!m_versionLabel) {
        return;
    }
    const int available = qMax(0, m_versionLabel->width());
    if (available <= 24) {
        m_versionLabel->setText(m_fullVersionText);
        return;
    }
    m_versionLabel->setText(
        m_versionLabel->fontMetrics().elidedText(m_fullVersionText,
                                                 Qt::ElideRight, available));
}
