/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "NavButton.h"

#include "Theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPropertyAnimation>

namespace {

QColor mix(const QColor &first, const QColor &second, qreal amount)
{
    const qreal t = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * t,
                            first.greenF() + (second.greenF() - first.greenF()) * t,
                            first.blueF() + (second.blueF() - first.blueF()) * t,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * t);
}

} // namespace

NavButton::NavButton(const QString &text, Icons::Type icon, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
    , m_hoverAnimation(new QPropertyAnimation(this, "hoverProgress", this))
    , m_activeAnimation(new QPropertyAnimation(this, "activeProgress", this))
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setFixedHeight(38);
    setFont(Theme::font(13, QFont::DemiBold));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_hoverAnimation->setDuration(160);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_activeAnimation->setDuration(260);
    m_activeAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void NavButton::setActive(bool active, bool animated)
{
    m_active = active;
    m_activeAnimation->stop();
    if (!animated) {
        setActiveProgress(active ? 1.0 : 0.0);
        return;
    }
    m_activeAnimation->setStartValue(m_activeProgress);
    m_activeAnimation->setEndValue(active ? 1.0 : 0.0);
    m_activeAnimation->start();
}

bool NavButton::isActive() const
{
    return m_active;
}

qreal NavButton::hoverProgress() const
{
    return m_hoverProgress;
}

void NavButton::setHoverProgress(qreal progress)
{
    m_hoverProgress = progress;
    update();
}

qreal NavButton::activeProgress() const
{
    return m_activeProgress;
}

void NavButton::setActiveProgress(qreal progress)
{
    m_activeProgress = progress;
    update();
}

QSize NavButton::sizeHint() const
{
    const QFontMetrics metrics(font());
    const int width = metrics.horizontalAdvance(text()) + 17 + 8 + 30;
    return QSize(width, 38);
}

QSize NavButton::minimumSizeHint() const
{
    return sizeHint();
}

void NavButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void NavButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}

void NavButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!isEnabled()) {
        painter.setOpacity(0.34);
    }

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 10.0;

    if (m_activeProgress < 0.99) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255,
                                static_cast<int>(20 * m_hoverProgress * (1.0 - m_activeProgress))));
        painter.drawRoundedRect(bounds, radius, radius);
    }

    const QColor idle = Theme::isDarkMode()
        ? mix(Theme::TextMuted, Theme::TextPrimary, 0.22 + 0.35 * m_hoverProgress)
        : QColor(255, 255, 255, static_cast<int>(215 + 35 * m_hoverProgress));
    const QColor color = mix(idle, Theme::Accent, m_activeProgress);

    const int iconSize = 17;
    const int gap = 8;
    const QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(text());
    const int contentWidth = iconSize + gap + textWidth;
    int x = qRound((width() - contentWidth) / 2.0);
    const int centerY = height() / 2;

    const QRectF iconRect(x, centerY - iconSize / 2.0, iconSize, iconSize);
    Icons::paint(painter, m_icon, iconRect, color, 1.65);
    x += iconSize + gap;

    painter.setFont(font());
    painter.setPen(color);
    painter.drawText(QRectF(x, 0, textWidth + 2, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, text());

    if (m_activeProgress > 0.05) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(),
                                static_cast<int>(190 * m_activeProgress)));
        const qreal dotX = width() / 2.0;
        painter.drawEllipse(QPointF(dotX, height() - 3.5), 2.0, 1.2);
    }

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Theme::isDarkMode()
                                ? QColor(Theme::Accent.red(), Theme::Accent.green(),
                                         Theme::Accent.blue(), 120)
                                : QColor(255, 255, 255, 150),
                            1.0));
        painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), radius - 1, radius - 1);
    }
}
