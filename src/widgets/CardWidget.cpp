/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "CardWidget.h"

#include "Theme.h"

#include <QEnterEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>

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

CardWidget::CardWidget(QWidget *parent)
    : QWidget(parent)
    , m_hoverAnimation(new QPropertyAnimation(this, "hoverProgress", this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_hoverAnimation->setDuration(220);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void CardWidget::setAccentColor(const QColor &color)
{
    m_accent = color;
    update();
}

QColor CardWidget::accentColor() const
{
    return m_accent;
}

void CardWidget::setInteractive(bool interactive)
{
    m_interactive = interactive;
    if (!interactive && m_hoverProgress > 0.0) {
        animateHover(0.0);
    }
}

bool CardWidget::isInteractive() const
{
    return m_interactive;
}

void CardWidget::setCornerRadius(qreal radius)
{
    m_cornerRadius = radius;
    update();
}

qreal CardWidget::cornerRadius() const
{
    return m_cornerRadius;
}

qreal CardWidget::hoverProgress() const
{
    return m_hoverProgress;
}

void CardWidget::setHoverProgress(qreal progress)
{
    m_hoverProgress = progress;
    update();
}

void CardWidget::animateHover(qreal target)
{
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(target);
    m_hoverAnimation->start();
}

void CardWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    if (m_interactive) {
        animateHover(1.0);
    }
}

void CardWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_interactive) {
        animateHover(0.0);
    }
}

void CardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(cardRect, m_cornerRadius, m_cornerRadius);
    painter.setClipPath(clip);

    QColor top = mix(Theme::Surface, m_accent, 0.012);
    QColor bottom = mix(Theme::SurfaceSoft, m_accent, 0.018);
    top = mix(top, m_accent, 0.018 * m_hoverProgress);

    QLinearGradient base(cardRect.topLeft(), cardRect.bottomLeft());
    base.setColorAt(0.0, top);
    base.setColorAt(1.0, bottom);
    painter.fillRect(cardRect, base);

    QRadialGradient glow(QPointF(cardRect.right() - cardRect.width() * 0.12,
                                 cardRect.top() + cardRect.height() * 0.05),
                         cardRect.width() * 0.7);
    glow.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(),
                                static_cast<int>(8 + 10 * m_hoverProgress)));
    glow.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 0));
    painter.fillRect(cardRect, glow);

    const QColor borderBase = Theme::isDarkMode()
        ? Theme::Border
        : QColor(23, 36, 58, 34);
    const QColor border = mix(borderBase, m_accent,
                              0.18 + 0.45 * m_hoverProgress);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(cardRect, m_cornerRadius, m_cornerRadius);
}
