/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "ToggleSwitch.h"

#include "Theme.h"

#include <QLinearGradient>
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

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_animation(new QPropertyAnimation(this, "knobPosition", this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(46, 26);
    setAttribute(Qt::WA_Hover, true);

    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        animateTo(checked ? 1.0 : 0.0);
    });
}

qreal ToggleSwitch::knobPosition() const
{
    return m_knobPosition;
}

void ToggleSwitch::setKnobPosition(qreal position)
{
    m_knobPosition = qBound(0.0, position, 1.0);
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(46, 26);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return sizeHint();
}

void ToggleSwitch::animateTo(qreal target)
{
    m_animation->stop();
    m_animation->setStartValue(m_knobPosition);
    m_animation->setEndValue(target);
    m_animation->start();
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = track.height() / 2.0;
    const bool on = isChecked();

    const QColor onStart(QStringLiteral("#4D97F2"));
    const QColor onEnd(QStringLiteral("#2F80ED"));
    const QColor offStart = Theme::SurfaceSoft;
    const QColor offEnd = Theme::Surface;

    QLinearGradient gradient(track.topLeft(), track.bottomLeft());
    if (on) {
        gradient.setColorAt(0.0, onStart);
        gradient.setColorAt(1.0, onEnd);
    } else {
        gradient.setColorAt(0.0, underMouse() ? mix(offStart, Qt::white, 0.05) : offStart);
        gradient.setColorAt(1.0, underMouse() ? mix(offEnd, Qt::white, 0.04) : offEnd);
    }

    painter.setBrush(gradient);
    painter.setPen(QPen(on ? QColor(255, 255, 255, 80) : Theme::Border, 1.0));
    painter.drawRoundedRect(track, radius, radius);

    if (on) {
        QLinearGradient shine(track.topLeft(), track.topRight());
        shine.setColorAt(0.0, QColor(255, 255, 255, 55));
        shine.setColorAt(0.5, QColor(255, 255, 255, 10));
        shine.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(shine);
        painter.drawRoundedRect(track.adjusted(1, 1, -1, -1), radius - 1, radius - 1);
    }

    const qreal knobRadius = track.height() / 2.0 - 3.0 - (isDown() ? 0.5 : 0.0);
    const qreal travel = track.width() - 6.0 - knobRadius * 2.0;
    const QPointF knobCenter(track.left() + 3.0 + knobRadius + travel * m_knobPosition,
                             track.center().y());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(23, 36, 58, on ? 35 : 24));
    painter.drawEllipse(knobCenter + QPointF(0, 1.0), knobRadius, knobRadius);

    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawEllipse(knobCenter, knobRadius, knobRadius);

    painter.setBrush(QColor(255, 255, 255, on ? 120 : 80));
    painter.drawEllipse(knobCenter - QPointF(knobRadius * 0.22, knobRadius * 0.30),
                        knobRadius * 0.34, knobRadius * 0.24);

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(), 115), 1.2));
        painter.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5), radius + 1.5, radius + 1.5);
    }
}
