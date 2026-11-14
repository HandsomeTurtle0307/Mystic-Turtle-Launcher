/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "TurtleAnimationWidget.h"

#include "Theme.h"

#include <QAbstractAnimation>
#include <QHideEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QtMath>

namespace {

constexpr qreal kPi = 3.14159265358979323846;

QPointF pointAt(const QPointF &center, qreal radius, qreal angle)
{
    return center + QPointF(qCos(angle) * radius, qSin(angle) * radius);
}

QPolygonF hexagon(const QPointF &center, qreal radius, qreal rotation)
{
    QPolygonF polygon;
    for (int i = 0; i < 6; ++i) {
        polygon.append(pointAt(center, radius,
                               rotation + i * (kPi / 3.0)));
    }
    return polygon;
}

} // namespace

TurtleAnimationWidget::TurtleAnimationWidget(QWidget *parent)
    : QWidget(parent)
    , m_animation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    m_animationsEnabled = Theme::animationsEnabled();

    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(12000);
    m_animation->setLoopCount(-1);
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { setProgress(value.toReal()); });
}

void TurtleAnimationWidget::setActive(bool active)
{
    if (m_active == active) {
        updateAnimationState();
        return;
    }
    m_active = active;
    updateAnimationState();
    update();
}

void TurtleAnimationWidget::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    updateAnimationState();
    update();
}

void TurtleAnimationWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateAnimationState();
}

void TurtleAnimationWidget::hideEvent(QHideEvent *event)
{
    m_animation->stop();
    QWidget::hideEvent(event);
}

void TurtleAnimationWidget::updateAnimationState()
{
    if (!m_active || !m_animationsEnabled || !isVisible()) {
        m_animation->stop();
        if (!m_animationsEnabled) {
            setProgress(0.18);
        }
        return;
    }
    if (m_animation->state() != QAbstractAnimation::Running) {
        m_animation->start();
    }
}

void TurtleAnimationWidget::setProgress(qreal progress)
{
    m_progress = progress;
    update();
}

void TurtleAnimationWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (width() <= 8 || height() <= 8) {
        return;
    }
    if (!m_active) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal phase = m_progress * 2.0 * kPi;
    const QPointF center(width() / 2.0,
                         height() / 2.0 + qSin(phase) * qMin(width(), height()) * 0.012);
    const qreal radius = qMin(width(), height()) * 0.235;

    QRadialGradient aura(center, radius * 2.7);
    QColor auraColor = Theme::Accent;
    auraColor.setAlphaF(0.055 + 0.025 * (0.5 + 0.5 * qSin(phase)));
    aura.setColorAt(0.0, auraColor);
    aura.setColorAt(0.62, QColor(Theme::Blue.red(), Theme::Blue.green(),
                                 Theme::Blue.blue(), 18));
    aura.setColorAt(1.0, QColor(Theme::Blue.red(), Theme::Blue.green(),
                                Theme::Blue.blue(), 0));
    painter.fillRect(rect(), aura);

    // Soft geometric water rings behind the turtle.
    painter.save();
    painter.translate(center);
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        QColor ring = i == 0 ? Theme::Accent : Theme::Blue;
        ring.setAlphaF(0.12 - i * 0.025);
        painter.setPen(QPen(ring, 1.0, Qt::DashLine));
        painter.drawEllipse(QPointF(0, 0),
                            radius * (1.55 + i * 0.22),
                            radius * (0.82 + i * 0.13));
    }

    for (int i = 0; i < 7; ++i) {
        const qreal angle = phase * 0.7 + i * (2.0 * kPi / 7.0);
        const qreal orbitRadius = radius * (1.38 + 0.14 * qSin(phase + i));
        QColor bubble = i % 2 == 0 ? Theme::Accent : Theme::Blue;
        bubble.setAlphaF(0.18 + 0.24 * (0.5 + 0.5 * qSin(angle * 1.9)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(bubble);
        painter.drawEllipse(pointAt(QPointF(0, 0), orbitRadius, angle),
                            1.5 + (i % 3) * 0.7, 1.5 + (i % 3) * 0.7);
    }
    painter.restore();

    painter.save();
    painter.translate(center);
    painter.rotate(qSin(phase) * 3.0);
    painter.scale(1.0 + qSin(phase * 2.0) * 0.010,
                  1.0 + qSin(phase * 2.0) * 0.010);

    const QColor flipperFill(110, 231, 183, 82);
    const QColor flipperOutline(190, 255, 231, 205);
    painter.setBrush(flipperFill);
    painter.setPen(QPen(flipperOutline, 1.6));

    const auto drawFlipper = [&painter, radius](const QPointF &position,
                                                 qreal angle, qreal squash) {
        painter.save();
        painter.translate(position);
        painter.rotate(angle);
        painter.drawEllipse(QPointF(0, 0), radius * 0.34,
                            radius * 0.135 * squash);
        painter.restore();
    };

    // Four paddle-shaped flippers, gently waving.
    drawFlipper(QPointF(-radius * 0.62, -radius * 0.54),
                -38.0 + qSin(phase * 2.0) * 4.0, 1.0);
    drawFlipper(QPointF(radius * 0.58, -radius * 0.54),
                38.0 + qSin(phase * 2.0 + 0.6) * 4.0, 1.0);
    drawFlipper(QPointF(-radius * 0.60, radius * 0.54),
                38.0 + qSin(phase * 2.0 + 1.2) * 4.0, 0.92);
    drawFlipper(QPointF(radius * 0.58, radius * 0.54),
                -38.0 + qSin(phase * 2.0 + 1.8) * 4.0, 0.92);

    // Tail.
    painter.setBrush(QColor(110, 231, 183, 62));
    painter.setPen(QPen(flipperOutline, 1.5));
    QPainterPath tail;
    tail.moveTo(-radius * 0.86, -radius * 0.15);
    tail.lineTo(-radius * 1.20, 0);
    tail.lineTo(-radius * 0.86, radius * 0.15);
    tail.closeSubpath();
    painter.drawPath(tail);

    // Oval shell with a visible rim.
    const QRectF shellRect(-radius * 1.02, -radius * 0.76,
                           radius * 2.04, radius * 1.52);
    QLinearGradient shellGradient(shellRect.topLeft(), shellRect.bottomRight());
    shellGradient.setColorAt(0.0, QColor(121, 238, 193, 118));
    shellGradient.setColorAt(0.62, QColor(68, 174, 155, 100));
    shellGradient.setColorAt(1.0, QColor(116, 153, 235, 84));
    painter.setBrush(shellGradient);
    painter.setPen(QPen(QColor(198, 255, 235, 220), 2.4));
    painter.drawEllipse(shellRect);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(220, 255, 245, 82), 1.2));
    painter.drawEllipse(shellRect.adjusted(radius * 0.10, radius * 0.10,
                                           -radius * 0.10, -radius * 0.10));

    // Simple scute pattern.
    const QPointF shellCenter(0, 0);
    QColor scute = QColor(215, 255, 240, 92);
    painter.setPen(QPen(scute, 1.0));
    painter.setBrush(QColor(110, 231, 183, 18));
    painter.drawPolygon(hexagon(shellCenter, radius * 0.37, kPi / 6.0));
    for (int i = 0; i < 6; ++i) {
        const qreal angle = kPi / 6.0 + i * (kPi / 3.0);
        const QPointF scuteCenter = pointAt(shellCenter, radius * 0.56, angle);
        painter.drawPolygon(hexagon(scuteCenter, radius * 0.18, kPi / 6.0));
        painter.drawLine(pointAt(shellCenter, radius * 0.37, angle),
                         pointAt(shellCenter, radius * 0.47, angle));
    }

    // Neck and head.
    painter.setBrush(QColor(110, 231, 183, 108));
    painter.setPen(QPen(QColor(198, 255, 235, 200), 1.6));
    painter.drawEllipse(QPointF(radius * 0.78, 0),
                        radius * 0.22, radius * 0.18);
    const QPointF head(radius * 0.99, -radius * 0.04);
    painter.setBrush(QColor(121, 238, 193, 125));
    painter.drawEllipse(head, radius * 0.27, radius * 0.23);

    // Eye and smile.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(240, 255, 249, 235));
    painter.drawEllipse(head + QPointF(radius * 0.07, -radius * 0.055),
                        radius * 0.055, radius * 0.060);
    painter.setBrush(QColor(8, 31, 26, 235));
    painter.drawEllipse(head + QPointF(radius * 0.09, -radius * 0.055),
                        radius * 0.024, radius * 0.030);
    painter.setBrush(QColor(255, 255, 255, 210));
    painter.drawEllipse(head + QPointF(radius * 0.10, -radius * 0.068),
                        radius * 0.010, radius * 0.010);

    QPainterPath smile;
    smile.moveTo(head + QPointF(radius * 0.04, radius * 0.065));
    smile.quadTo(head + QPointF(radius * 0.10, radius * 0.115),
                 head + QPointF(radius * 0.16, radius * 0.060));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(18, 62, 52, 185), 1.3,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(smile);

    // A tiny shell highlight keeps the silhouette readable.
    painter.setPen(QPen(QColor(255, 255, 255, 68), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(-radius * 0.45, -radius * 0.54,
                           radius * 0.90, radius * 0.70),
                    -150 * 16, 55 * 16);

    painter.restore();
}
