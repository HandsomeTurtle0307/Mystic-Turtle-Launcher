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
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QCursor>

WindowButton::WindowButton(Icons::Type icon, bool danger, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
    , m_danger(danger)
    , m_hoverAnimation(new QPropertyAnimation(this, "hoverProgress", this))
    , m_spotlightAnimation(new QPropertyAnimation(this, "spotlightProgress", this))
{
    setFixedSize(44, 32);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);

    m_hoverAnimation->setDuration(150);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_spotlightAnimation->setDuration(200);
    m_spotlightAnimation->setEasingCurve(QEasingCurve::OutCubic);
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

qreal WindowButton::spotlightProgress() const
{
    return m_spotlightProgress;
}

void WindowButton::setSpotlightProgress(qreal progress)
{
    m_spotlightProgress = qBound(0.0, progress, 1.0);
    update();
}

QSize WindowButton::sizeHint() const
{
    return QSize(44, 32);
}

void WindowButton::animateHover(qreal target)
{
    m_hoverAnimation->stop();
    if (!Theme::animationsEnabled()) {
        setHoverProgress(target);
        return;
    }
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(target);
    m_hoverAnimation->start();
}

void WindowButton::animateSpotlight(qreal target)
{
    m_spotlightAnimation->stop();
    if (!Theme::animationsEnabled()) {
        setSpotlightProgress(target);
        return;
    }
    m_spotlightAnimation->setStartValue(m_spotlightProgress);
    m_spotlightAnimation->setEndValue(target);
    m_spotlightAnimation->start();
}

void WindowButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    m_spotlightPosition = mapFromGlobal(QCursor::pos());
    animateHover(1.0);
    if (Theme::buttonSpotlightEnabled()) {
        animateSpotlight(1.0);
    }
}

void WindowButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateHover(0.0);
    animateSpotlight(0.0);
}

void WindowButton::mouseMoveEvent(QMouseEvent *event)
{
    m_spotlightPosition = event->position();
    if (Theme::buttonSpotlightEnabled()) {
        update();
    }
    QAbstractButton::mouseMoveEvent(event);
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

    if (Theme::buttonSpotlightEnabled() && m_spotlightProgress > 0.001) {
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(bounds, 7, 7);
        painter.setClipPath(clip);
        const QColor tint = m_danger ? Theme::Danger : Theme::Accent;
        QRadialGradient light(m_spotlightPosition, qMax(46.0, width() * 1.1));
        light.setColorAt(0.0, QColor(255, 255, 255,
                                     static_cast<int>(50 * m_spotlightProgress)));
        light.setColorAt(0.35, QColor(tint.red(), tint.green(), tint.blue(),
                                      static_cast<int>(22 * m_spotlightProgress)));
        light.setColorAt(0.7, QColor(tint.red(), tint.green(), tint.blue(),
                                     static_cast<int>(5 * m_spotlightProgress)));
        light.setColorAt(1.0, QColor(tint.red(), tint.green(), tint.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.fillRect(bounds, light);
        painter.restore();
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
