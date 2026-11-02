/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 */

#include "LoadingIndicator.h"

#include "Theme.h"

#include <QHideEvent>
#include <QPainter>
#include <QShowEvent>
#include <QTimer>

LoadingIndicator::LoadingIndicator(QWidget *parent, const QColor &color)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_color(color)
    , m_animationsEnabled(Theme::animationsEnabled())
{
    setFixedSize(24, 24);
    m_timer->setInterval(35);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 30) % 360;
        update();
    });
}

void LoadingIndicator::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (enabled && isVisible()) {
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

void LoadingIndicator::setColor(const QColor &color)
{
    if (m_color == color) {
        return;
    }
    m_color = color;
    update();
}

void LoadingIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF circle = QRectF(rect()).adjusted(3, 3, -3, -3);
    QColor track = m_color;
    track.setAlpha(48);
    painter.setPen(QPen(track, 3));
    painter.drawEllipse(circle);
    painter.setPen(QPen(m_color, 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(circle, m_angle * 16, 95 * 16);
}

void LoadingIndicator::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_animationsEnabled) {
        m_timer->start();
    }
}

void LoadingIndicator::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}
