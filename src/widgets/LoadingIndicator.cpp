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
#include <QtMath>

LoadingIndicator::LoadingIndicator(QWidget *parent, const QColor &color)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_color(color)
    , m_animationsEnabled(Theme::animationsEnabled())
{
    setFixedSize(28, 28);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 5) % 360;
        m_ripplePhase += 0.018;
        if (m_ripplePhase >= 1.0) {
            m_ripplePhase -= 1.0;
        }
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
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal side = qMin(width(), height());
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal discRadius = side * 0.27;
    const qreal ringRadius = side * 0.38;
    const qreal outerRadius = side * 0.49;

    // Expanding water ripples. They stay low-alpha so the control still reads
    // as a quiet loading indicator rather than a glowing decoration.
    if (m_animationsEnabled) {
        for (int index = 0; index < 3; ++index) {
            qreal phase = m_ripplePhase + index / 3.0;
            phase -= qFloor(phase);
            const qreal radius = ringRadius + (outerRadius - ringRadius) * phase;
            QColor ripple = m_color;
            ripple.setAlphaF(0.16 * (1.0 - phase));
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(ripple, 1.1));
            painter.drawEllipse(center, radius, radius);
        }
    }

    QColor disc = Theme::isDarkMode() ? Theme::SurfaceRaised
                                      : QColor(255, 255, 255, 210);
    disc.setAlpha(Theme::isDarkMode() ? 220 : 210);
    painter.setBrush(disc);
    QColor discBorder = m_color;
    discBorder.setAlpha(42);
    painter.setPen(QPen(discBorder, 1.0));
    painter.drawEllipse(center, discRadius, discRadius);

    QColor track = m_color;
    track.setAlpha(46);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(track, 2.0));
    painter.drawEllipse(center, ringRadius, ringRadius);

    painter.setPen(QPen(m_color, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(center.x() - ringRadius, center.y() - ringRadius,
                           ringRadius * 2.0, ringRadius * 2.0),
                    m_angle * 16, 100 * 16);

    QColor hub = m_color;
    hub.setAlpha(210);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hub);
    painter.drawEllipse(center, qMax(1.4, side * 0.045),
                        qMax(1.4, side * 0.045));

    // A quiet gauge needle inside the disc echoes the dashboard visual while
    // the outer arc and water ripples provide the continuous motion.
    const qreal needleAngle = qDegreesToRadians(qreal(m_angle) - 90.0);
    const QPointF needleEnd(center.x() + qCos(needleAngle) * discRadius * 0.74,
                            center.y() + qSin(needleAngle) * discRadius * 0.74);
    QColor needle = m_color;
    needle.setAlpha(220);
    painter.setPen(QPen(needle, qMax(1.2, side * 0.045),
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, needleEnd);
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
