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

#include <QPainter>
#include <QPropertyAnimation>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_animation(new QPropertyAnimation(this, "knobPosition", this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(46, 26);
    setAttribute(Qt::WA_Hover, true);

    m_animation->setDuration(Theme::MotionNormal);
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
    if (!Theme::animationsEnabled()) {
        setKnobPosition(target);
        return;
    }
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

    QColor trackColor = on
        ? Theme::Accent
        : (underMouse() ? QColor(QStringLiteral("#303B48")) : Theme::SurfaceSoft);
    QColor borderColor = on
        ? QColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(), 180)
        : Theme::Border;

    painter.setBrush(trackColor);
    painter.setPen(QPen(borderColor, 1.0));
    painter.drawRoundedRect(track, radius, radius);

    const qreal knobRadius = track.height() / 2.0 - 3.0;
    const qreal travel = track.width() - 6.0 - knobRadius * 2.0;
    const QPointF knobCenter(track.left() + 3.0 + knobRadius + travel * m_knobPosition,
                             track.center().y());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    painter.drawEllipse(knobCenter, knobRadius, knobRadius);

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                   Theme::Accent.blue(), 130), 1.2));
        painter.drawRoundedRect(track.adjusted(1.5, 1.5, -1.5, -1.5),
                                qMax(1.0, radius - 1.5),
                                qMax(1.0, radius - 1.5));
    }
}
