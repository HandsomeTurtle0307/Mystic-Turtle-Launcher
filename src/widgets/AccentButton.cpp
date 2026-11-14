/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AccentButton.h"

#include "Theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QCursor>

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

AccentButton::AccentButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
    , m_hoverAnimation(new QPropertyAnimation(this, "hoverProgress", this))
    , m_pressAnimation(new QPropertyAnimation(this, "pressProgress", this))
    , m_spotlightAnimation(new QPropertyAnimation(this, "spotlightProgress", this))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFlat(true);
    setAutoDefault(false);
    setDefault(false);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedHeight(38);
    setFont(paintFont());

    m_hoverAnimation->setDuration(170);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnimation->setDuration(90);
    m_pressAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_spotlightAnimation->setDuration(240);
    m_spotlightAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QAbstractButton::toggled, this, [this]() { update(); });
    connect(this, &QAbstractButton::pressed, this, [this]() { animatePress(1.0); });
    connect(this, &QAbstractButton::released, this, [this]() { animatePress(0.0); });
}

AccentButton::AccentButton(const QString &text, Icons::Type icon, QWidget *parent)
    : AccentButton(text, parent)
{
    setIconType(icon);
}

void AccentButton::setVariant(Variant variant)
{
    m_variant = variant;
    update();
}

AccentButton::Variant AccentButton::variant() const
{
    return m_variant;
}

void AccentButton::setIconType(Icons::Type type)
{
    m_iconType = type;
    m_hasIcon = true;
    updateGeometry();
    update();
}

void AccentButton::clearIcon()
{
    m_hasIcon = false;
    updateGeometry();
    update();
}

bool AccentButton::hasIcon() const
{
    return m_hasIcon;
}

void AccentButton::setAccentColor(const QColor &color)
{
    m_accent = color;
    update();
}

QColor AccentButton::accentColor() const
{
    return m_accent;
}

void AccentButton::setCompact(bool compact)
{
    if (m_compact == compact) {
        return;
    }
    m_compact = compact;
    setFixedHeight(compact ? 32 : 38);
    setFont(paintFont());
    updateGeometry();
    update();
}

bool AccentButton::isCompact() const
{
    return m_compact;
}

qreal AccentButton::hoverProgress() const
{
    return m_hoverProgress;
}

void AccentButton::setHoverProgress(qreal progress)
{
    m_hoverProgress = progress;
    update();
}

qreal AccentButton::pressProgress() const
{
    return m_pressProgress;
}

void AccentButton::setPressProgress(qreal progress)
{
    m_pressProgress = qBound(0.0, progress, 1.0);
    update();
}

qreal AccentButton::spotlightProgress() const
{
    return m_spotlightProgress;
}

void AccentButton::setSpotlightProgress(qreal progress)
{
    m_spotlightProgress = qBound(0.0, progress, 1.0);
    update();
}

QFont AccentButton::paintFont() const
{
    return Theme::font(m_compact ? 12 : 13, QFont::DemiBold);
}

QSize AccentButton::sizeHint() const
{
    const QFontMetrics metrics(paintFont());
    const int horizontalPadding = m_compact ? 14 : 18;
    const int iconSize = m_compact ? 15 : 16;
    int width = metrics.horizontalAdvance(text()) + horizontalPadding * 2;
    if (m_hasIcon) {
        width += iconSize + (text().isEmpty() ? 0 : 8);
    }
    if (width < 38) {
        width = 38;
    }
    return QSize(width, m_compact ? 32 : 38);
}

QSize AccentButton::minimumSizeHint() const
{
    return sizeHint();
}

void AccentButton::animateHover(qreal target)
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

void AccentButton::animatePress(qreal target)
{
    m_pressAnimation->stop();
    if (!Theme::animationsEnabled()) {
        setPressProgress(target);
        return;
    }
    m_pressAnimation->setStartValue(m_pressProgress);
    m_pressAnimation->setEndValue(target);
    m_pressAnimation->start();
}

void AccentButton::animateSpotlight(qreal target)
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

void AccentButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    m_spotlightPosition = mapFromGlobal(QCursor::pos());
    if (isEnabled()) {
        animateHover(1.0);
        if (Theme::buttonSpotlightEnabled()) {
            animateSpotlight(1.0);
        }
    }
}

void AccentButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    animateHover(0.0);
    animateSpotlight(0.0);
}

void AccentButton::mouseMoveEvent(QMouseEvent *event)
{
    m_spotlightPosition = event->position();
    if (Theme::buttonSpotlightEnabled()) {
        update();
    }
    QPushButton::mouseMoveEvent(event);
}

void AccentButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal scale = 1.0 - 0.045 * m_pressProgress;
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(scale, scale);
    painter.translate(-width() / 2.0, -height() / 2.0);

    const qreal radius = m_compact ? height() / 2.0 : 10.0;
    const QRectF buttonRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool checked = isChecked();
    const bool enabled = isEnabled();
    const qreal hover = enabled ? m_hoverProgress : 0.0;

    QColor background;
    QColor border;
    QColor foreground;

    switch (m_variant) {
    case Variant::Primary:
        background = mix(m_accent, Theme::AccentStrong, 0.22 + 0.15 * hover);
        border = QColor(0, 0, 0, 0);
        foreground = Theme::isDarkMode() ? QColor(QStringLiteral("#06120E"))
                                         : QColor(QStringLiteral("#FFFFFF"));
        break;
    case Variant::Secondary:
        background = checked
            ? QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 24)
            : QColor(0, 0, 0, 0);
        if (!checked && hover > 0.0) {
            background = QColor(m_accent.red(), m_accent.green(), m_accent.blue(),
                                static_cast<int>(18 * hover));
        }
        border = checked
            ? QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 120)
            : Theme::Border;
        foreground = checked ? m_accent
                             : mix(Theme::TextSecondary, Theme::TextPrimary, 0.45 + 0.55 * hover);
        break;
    case Variant::Ghost:
        background = checked
            ? QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 22)
            : QColor(m_accent.red(), m_accent.green(), m_accent.blue(),
                     static_cast<int>(12 * hover));
        border = checked
            ? QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 80)
            : QColor(0, 0, 0, 0);
        foreground = checked ? m_accent
                             : mix(Theme::TextSecondary, Theme::TextPrimary, 0.38 + 0.62 * hover);
        break;
    case Variant::Danger:
        background = QColor(Theme::Danger.red(), Theme::Danger.green(), Theme::Danger.blue(),
                            static_cast<int>(14 + 20 * hover));
        border = QColor(Theme::Danger.red(), Theme::Danger.green(), Theme::Danger.blue(),
                        static_cast<int>(55 + 55 * hover));
        foreground = Theme::Danger;
        break;
    }

    if (!enabled) {
        background = Theme::SurfaceSoft;
        border = Theme::Border;
        foreground = Theme::TextMuted;
    }

    if (m_variant == Variant::Primary && enabled) {
        QLinearGradient fill(buttonRect.topLeft(), buttonRect.bottomLeft());
        fill.setColorAt(0.0, mix(background, Qt::white, 0.08));
        fill.setColorAt(1.0, mix(background, Theme::AccentStrong, 0.16));
        painter.setBrush(fill);
    } else {
        painter.setBrush(background);
    }

    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(buttonRect, radius, radius);

    if (enabled && hover > 0.01) {
        QLinearGradient sheen(buttonRect.topLeft(), buttonRect.bottomLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(32 * hover)));
        sheen.setColorAt(0.55, QColor(255, 255, 255, 0));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(sheen);
        painter.drawRoundedRect(buttonRect.adjusted(1.0, 1.0, -1.0, -1.0),
                                qMax(1.0, radius - 1.0), qMax(1.0, radius - 1.0));

        QColor glow = m_variant == Variant::Danger ? Theme::Danger : m_accent;
        glow.setAlphaF(0.22 * hover);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(glow, 1.0 + 0.8 * hover));
        painter.drawRoundedRect(buttonRect.adjusted(1.2, 1.2, -1.2, -1.2),
                                qMax(1.0, radius - 1.2), qMax(1.0, radius - 1.2));
    }

    if (enabled && Theme::liquidGlassEnabled()) {
        QLinearGradient glassEdge(buttonRect.topLeft(), buttonRect.bottomLeft());
        glassEdge.setColorAt(0.0, QColor(255, 255, 255, 22));
        glassEdge.setColorAt(0.42, QColor(255, 255, 255, 4));
        glassEdge.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(),
                                         m_accent.blue(), 12));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glassEdge);
        painter.drawRoundedRect(buttonRect.adjusted(0.8, 0.8, -0.8, -0.8),
                                qMax(1.0, radius - 0.8), qMax(1.0, radius - 0.8));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 24), 0.9));
        painter.drawLine(QPointF(buttonRect.left() + radius,
                                 buttonRect.top() + 1.1),
                         QPointF(buttonRect.right() - radius,
                                 buttonRect.top() + 1.1));
    }

    if (enabled && Theme::buttonSpotlightEnabled()
        && m_spotlightProgress > 0.001) {
        painter.save();
        QPainterPath spotlightClip;
        spotlightClip.addRoundedRect(buttonRect, radius, radius);
        painter.setClipPath(spotlightClip);
        const QColor tint = m_variant == Variant::Danger ? Theme::Danger : m_accent;
        const qreal lightRadius = qMax(64.0, qMax(width(), height()) * 1.15);
        QRadialGradient light(m_spotlightPosition, lightRadius);
        light.setColorAt(0.0, QColor(255, 255, 255,
                                     static_cast<int>(46 * m_spotlightProgress)));
        light.setColorAt(0.28, QColor(tint.red(), tint.green(), tint.blue(),
                                      static_cast<int>(22 * m_spotlightProgress)));
        light.setColorAt(0.62, QColor(tint.red(), tint.green(), tint.blue(),
                                      static_cast<int>(6 * m_spotlightProgress)));
        light.setColorAt(1.0, QColor(tint.red(), tint.green(), tint.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.fillRect(buttonRect, light);
        painter.restore();
    }

    if (hasFocus() && focusPolicy() != Qt::NoFocus) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 130), 1.2));
        painter.drawRoundedRect(buttonRect.adjusted(1.8, 1.8, -1.8, -1.8),
                                qMax(1.0, radius - 1.8), qMax(1.0, radius - 1.8));
    }

    const int iconSize = m_compact ? 15 : 16;
    const int gap = text().isEmpty() ? 0 : 8;
    const QFontMetricsF metrics(paintFont());
    const qreal textWidth = metrics.horizontalAdvance(text());
    const qreal contentWidth = (m_hasIcon ? iconSize : 0) + gap + textWidth;
    const qreal contentLeft = (width() - contentWidth) / 2.0;
    qreal cursorX = contentLeft;
    int offsetY = enabled ? qRound(m_pressProgress) : 0;

    if (m_hasIcon) {
        const QRectF iconRect(qRound(cursorX), (height() - iconSize) / 2.0 + offsetY,
                              iconSize, iconSize);
        Icons::paint(painter, m_iconType, iconRect, foreground, 1.7);
        cursorX += iconSize + gap;
    }

    if (!text().isEmpty()) {
        painter.setFont(paintFont());
        painter.setPen(foreground);
        const QRectF textRect(cursorX, offsetY, textWidth, height());
        painter.drawText(textRect, Qt::AlignCenter, text());
    }
}
