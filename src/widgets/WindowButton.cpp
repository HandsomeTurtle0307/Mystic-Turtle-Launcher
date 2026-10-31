/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "WindowButton.h"

#include "Theme.h"

#include <QEnterEvent>
#include <QPainter>
#include <QPropertyAnimation>

WindowButton::WindowButton(Icons::Type icon, bool danger, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
    , m_danger(danger)
    , m_hoverAnimation(new QPropertyAnimation(this, "hoverProgress", this))
{
    setFixedSize(44, 32);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);

    m_hoverAnimation->setDuration(150);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void WindowButton::setIconType(Icons::Type type)
{
    m_icon = type;
    update();
}

Icons::Type WindowButton::iconType() const
{
    return m_icon;
}

void WindowButton::setDanger(bool danger)
{
    m_danger = danger;
    update();
}

bool WindowButton::isDanger() const
{
    return m_danger;
}

qreal WindowButton::hoverProgress() const
{
    return m_hoverProgress;
}

void WindowButton::setHoverProgress(qreal progress)
{
    m_hoverProgress = progress;
    update();
}

QSize WindowButton::sizeHint() const
{
    return QSize(44, 32);
}

void WindowButton::animateHover(qreal target)
{
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(target);
    m_hoverAnimation->start();
}

void WindowButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    animateHover(1.0);
}

void WindowButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateHover(0.0);
}

void WindowButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(2, 2, -2, -2);
    if (m_hoverProgress > 0.001) {
        const QColor background = m_danger
            ? QColor(229, 72, 77, static_cast<int>(235 * m_hoverProgress))
            : QColor(255, 255, 255, static_cast<int>(38 * m_hoverProgress));
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(bounds, 7, 7);
    }

    QColor iconColor = m_danger && m_hoverProgress > 0.4
        ? QColor(QStringLiteral("#FFFFFF"))
        : QColor(255, 255, 255, static_cast<int>(220 + 35 * m_hoverProgress));

    if (isDown()) {
        iconColor = iconColor.lighter(115);
    }

    const QRectF iconRect((width() - 14) / 2.0, (height() - 14) / 2.0, 14, 14);
    Icons::paint(painter, m_icon, iconRect, iconColor, 1.5);
}
