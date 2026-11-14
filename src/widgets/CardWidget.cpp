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

#include <QBrush>
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
    if (!Theme::animationsEnabled()) {
        setHoverProgress(target);
        return;
    }
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

    const bool glass = Theme::liquidGlassEnabled();
    if (glass) {
        // Layered translucent fills imitate a thick sheet of liquid glass:
        // a cool top gradient, a soft ambient bloom and a bright rim.
        QLinearGradient base(cardRect.topLeft(), cardRect.bottomLeft());
        base.setColorAt(0.0, QColor(255, 255, 255,
                                     Theme::isDarkMode() ? 12 : 38));
        base.setColorAt(0.42, QColor(255, 255, 255,
                                      Theme::isDarkMode() ? 3 : 12));
        base.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                    m_accent.blue(),
                                    Theme::isDarkMode() ? 5 : 12));
        painter.fillRect(cardRect, base);

        QRadialGradient bloom(QPointF(cardRect.left() + cardRect.width() * 0.18,
                                      cardRect.top() + 22.0),
                              qMax(120.0, cardRect.width() * 0.7));
        QColor bloomColor = m_accent;
        bloomColor.setAlphaF(Theme::isDarkMode() ? 0.055 : 0.09);
        bloom.setColorAt(0.0, bloomColor);
        bloom.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                     m_accent.blue(), 0));
        painter.fillRect(cardRect, bloom);

        const QRectF specular = cardRect.adjusted(1.0, 1.0, -1.0, -1.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255,
                                   Theme::isDarkMode() ? 26 : 72), 1.0));
        painter.drawLine(QPointF(specular.left() + m_cornerRadius,
                                 specular.top() + 0.7),
                         QPointF(specular.right() - m_cornerRadius,
                                 specular.top() + 0.7));

        QLinearGradient edge(cardRect.topLeft(), cardRect.bottomRight());
        edge.setColorAt(0.0, QColor(255, 255, 255, 30));
        edge.setColorAt(0.38, QColor(255, 255, 255, 0));
        edge.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                    m_accent.blue(), 20));
        painter.setPen(QPen(QBrush(edge), 1.0));
        painter.drawRoundedRect(cardRect.adjusted(0.7, 0.7, -0.7, -0.7),
                                m_cornerRadius - 0.7, m_cornerRadius - 0.7);
    }

    const QColor borderBase = Theme::isDarkMode()
        ? Theme::Border
        : QColor(23, 36, 58, 34);

    if (m_hoverProgress > 0.01) {
        const qreal hover = qBound(0.0, m_hoverProgress, 1.0);

        QColor sheenColor = m_accent;
        sheenColor.setAlphaF(0.075 * hover);
        QColor sheenFade = m_accent;
        sheenFade.setAlpha(0);
        QLinearGradient sheen(cardRect.topLeft(), cardRect.bottomRight());
        sheen.setColorAt(0.0, sheenColor);
        sheen.setColorAt(0.45, sheenFade);
        sheen.setColorAt(1.0, sheenFade);
        painter.fillRect(cardRect, sheen);

        QRadialGradient halo(QPointF(cardRect.right() - 52.0,
                                     cardRect.top() + 26.0),
                              qMax(80.0, cardRect.width() * 0.32));
        QColor haloColor = m_accent;
        haloColor.setAlphaF(0.11 * hover);
        halo.setColorAt(0.0, haloColor);
        halo.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                   m_accent.blue(), 0));
        painter.fillRect(cardRect, halo);

        QColor ring = m_accent;
        ring.setAlphaF(0.22 * hover);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ring, 1.0 + 0.8 * hover));
        painter.drawRoundedRect(cardRect.adjusted(1.0, 1.0, -1.0, -1.0),
                                m_cornerRadius - 1.0, m_cornerRadius - 1.0);

        QLinearGradient topEdge(cardRect.topLeft(), cardRect.topRight());
        QColor edge = m_accent;
        edge.setAlphaF(0.38 * hover);
        topEdge.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(),
                                      m_accent.blue(), 0));
        topEdge.setColorAt(0.28, edge);
        topEdge.setColorAt(0.72, edge);
        topEdge.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                      m_accent.blue(), 0));
        painter.setPen(QPen(QBrush(topEdge), 1.2));
        painter.drawLine(QPointF(cardRect.left() + m_cornerRadius,
                                 cardRect.top() + 1.0),
                         QPointF(cardRect.right() - m_cornerRadius,
                                 cardRect.top() + 1.0));
    }

    const QColor border = mix(borderBase, m_accent,
                              0.18 + 0.45 * m_hoverProgress);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(cardRect, m_cornerRadius, m_cornerRadius);
}
