/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 */

#include "ModernInputs.h"

#include "IconFactory.h"
#include "Theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QAbstractItemView>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionComboBox>
#include <QVariantAnimation>

namespace {

QColor mix(const QColor &first, const QColor &second, qreal amount)
{
    const qreal t = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * t,
                            first.greenF() + (second.greenF() - first.greenF()) * t,
                            first.blueF() + (second.blueF() - first.blueF()) * t,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * t);
}

QColor surfaceFill(qreal progress)
{
    if (!Theme::isDarkMode()) {
        return QColor(255, 255, 255, 236);
    }
    return QColor(255, 255, 255, int(6 + 5 * progress));
}

QColor surfaceBorder(qreal progress)
{
    if (!Theme::isDarkMode()) {
        return QColor(23, 36, 58, int(40 + 55 * progress));
    }
    return QColor(255, 255, 255, int(22 + 40 * progress));
}

} // namespace

ModernLineEdit::ModernLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setFrame(false);
    setAttribute(Qt::WA_Hover, true);
    setMinimumHeight(34);
    setStyleSheet(QStringLiteral(
        "QLineEdit { background: transparent; border: none; padding: 7px 10px;"
        "color: %1; selection-background-color: rgba(110, 231, 183, 90);"
        "selection-color: #FFFFFF; }"
        "QLineEdit:disabled { color: %2; }")
        .arg(Theme::TextPrimary.name(), Theme::TextMuted.name()));
    QPalette palette = this->palette();
    palette.setColor(QPalette::PlaceholderText, Theme::TextMuted);
    setPalette(palette);

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
        m_progress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
}

ModernLineEdit::ModernLineEdit(const QString &text, QWidget *parent)
    : ModernLineEdit(parent)
{
    setText(text);
}

qreal ModernLineEdit::targetProgress() const
{
    if (!isEnabled()) {
        return 0.0;
    }
    if (hasFocus()) {
        return 1.0;
    }
    return underMouse() ? 0.45 : 0.0;
}

void ModernLineEdit::animateProgress(qreal target)
{
    if (!Theme::animationsEnabled()) {
        m_animation->stop();
        m_progress = target;
        update();
        return;
    }
    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->start();
}

void ModernLineEdit::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    animateProgress(targetProgress());
}

void ModernLineEdit::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    animateProgress(targetProgress());
}

void ModernLineEdit::enterEvent(QEnterEvent *event)
{
    QLineEdit::enterEvent(event);
    animateProgress(targetProgress());
}

void ModernLineEdit::leaveEvent(QEvent *event)
{
    QLineEdit::leaveEvent(event);
    animateProgress(targetProgress());
}

void ModernLineEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor fill = isEnabled() ? surfaceFill(m_progress)
                                    : (Theme::isDarkMode()
                                           ? QColor(255, 255, 255, 3)
                                           : QColor(255, 255, 255, 160));
    QColor border = isEnabled() ? surfaceBorder(m_progress)
                                : (Theme::isDarkMode()
                                       ? QColor(255, 255, 255, 14)
                                       : QColor(23, 36, 58, 24));
    if (hasFocus() && isEnabled()) {
        border = mix(border, Theme::Accent, 0.62 * m_progress);
        border.setAlpha(Theme::isDarkMode()
                            ? int(65 + 70 * m_progress)
                            : int(70 + 65 * m_progress));
    }

    painter.setBrush(fill);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(bounds, 9.0, 9.0);
    painter.end();

    QLineEdit::paintEvent(event);
}

ModernComboBox::ModernComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setMinimumHeight(34);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(QStringLiteral(
        "QComboBox { background: transparent; border: none; padding: 7px 30px 7px 11px;"
        "color: %1; }"
        "QComboBox::drop-down { border: none; width: 0px; }"
        "QComboBox QAbstractItemView {"
        "background: %2; color: %1; border: 1px solid %3;"
        "border-radius: 8px; padding: 4px; outline: 0;"
        "selection-background-color: rgba(110, 231, 183, 28);"
        "selection-color: #FFFFFF; }"
        "QComboBox QAbstractItemView::item { min-height: 26px; padding: 3px 8px;"
        "border-radius: 5px; }")
        .arg(Theme::TextPrimary.name(),
             Theme::isDarkMode() ? QStringLiteral("#151C24") : QStringLiteral("#FFFFFF"),
             Theme::isDarkMode() ? QStringLiteral("rgba(255, 255, 255, 24)")
                                  : QStringLiteral("#D7E3F2")));
    QPalette palette = this->palette();
    palette.setColor(QPalette::PlaceholderText, Theme::TextMuted);
    setPalette(palette);

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
        m_progress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
}

qreal ModernComboBox::targetProgress() const
{
    if (!isEnabled()) {
        return 0.0;
    }
    if (hasFocus()) {
        return 1.0;
    }
    return underMouse() ? 0.45 : 0.0;
}

void ModernComboBox::animateProgress(qreal target)
{
    if (!Theme::animationsEnabled()) {
        m_animation->stop();
        m_progress = target;
        update();
        return;
    }
    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->start();
}

void ModernComboBox::focusInEvent(QFocusEvent *event)
{
    QComboBox::focusInEvent(event);
    animateProgress(targetProgress());
}

void ModernComboBox::focusOutEvent(QFocusEvent *event)
{
    QComboBox::focusOutEvent(event);
    animateProgress(targetProgress());
}

void ModernComboBox::enterEvent(QEnterEvent *event)
{
    QComboBox::enterEvent(event);
    animateProgress(targetProgress());
}

void ModernComboBox::leaveEvent(QEvent *event)
{
    QComboBox::leaveEvent(event);
    animateProgress(targetProgress());
}

void ModernComboBox::showPopup()
{
    QComboBox::showPopup();
    QWidget *popup = view() ? view()->window() : nullptr;
    if (!popup || !Theme::animationsEnabled()) {
        return;
    }

    auto *effect = new QGraphicsOpacityEffect(popup);
    effect->setOpacity(0.35);
    popup->setGraphicsEffect(effect);
    auto *fade = new QPropertyAnimation(effect, "opacity", popup);
    fade->setDuration(140);
    fade->setStartValue(0.35);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(fade, &QPropertyAnimation::finished, effect,
            [effect]() { effect->setOpacity(1.0); });
    connect(fade, &QPropertyAnimation::finished, fade,
            &QObject::deleteLater);
    fade->start();
}

void ModernComboBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool enabled = isEnabled();
    const QColor fill = enabled ? surfaceFill(m_progress)
                                : (Theme::isDarkMode()
                                       ? QColor(255, 255, 255, 3)
                                       : QColor(255, 255, 255, 160));
    QColor border = enabled ? surfaceBorder(m_progress)
                            : (Theme::isDarkMode()
                                   ? QColor(255, 255, 255, 14)
                                   : QColor(23, 36, 58, 24));
    if (hasFocus() && enabled) {
        border = mix(border, Theme::Accent, 0.62 * m_progress);
        border.setAlpha(Theme::isDarkMode()
                            ? int(65 + 70 * m_progress)
                            : int(70 + 65 * m_progress));
    }
    painter.setBrush(fill);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(bounds, 9.0, 9.0);

    const int leftPadding = 11;
    int contentLeft = leftPadding;
    const QIcon icon = itemIcon(currentIndex());
    if (!icon.isNull()) {
        const QRectF iconRect(contentLeft, (height() - 16) / 2.0, 16, 16);
        icon.paint(&painter, iconRect.toRect());
        contentLeft += 22;
    }

    const QRect textRect(contentLeft, 0, qMax(0, width() - contentLeft - 32), height());
    const QColor textColor = enabled ? Theme::TextPrimary : Theme::TextMuted;
    painter.setPen(textColor);
    painter.setFont(font());
    if (!isEditable()) {
        const QString label = fontMetrics().elidedText(currentText(),
                                                        Qt::ElideRight,
                                                        textRect.width());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    Icons::paint(painter, Icons::Type::ChevronDown,
                 QRectF(width() - 25, (height() - 16) / 2.0, 16, 16),
                 enabled ? mix(Theme::TextMuted, Theme::TextPrimary, 0.55)
                         : Theme::TextMuted,
                 1.6);
}
