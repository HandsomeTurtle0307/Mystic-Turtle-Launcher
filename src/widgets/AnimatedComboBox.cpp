/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 */

#include "AnimatedComboBox.h"

#include "Theme.h"

#include <QAbstractItemView>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

AnimatedComboBox::AnimatedComboBox(QWidget *parent)
    : ModernComboBox(parent)
{
}

void AnimatedComboBox::showPopup()
{
    ModernComboBox::showPopup();
    if (!Theme::animationsEnabled() || !view()) {
        return;
    }

    QWidget *popup = view()->window();
    if (!popup || popup == this) {
        return;
    }

    if (m_expandAnimation) {
        m_expandAnimation->stop();
        m_expandAnimation->deleteLater();
    }

    const int targetHeight = view()->height();
    view()->setMaximumHeight(0);
    auto *expandAnimation = new QPropertyAnimation(view(), "maximumHeight", popup);
    m_expandAnimation = expandAnimation;
    expandAnimation->setDuration(180);
    expandAnimation->setStartValue(0);
    expandAnimation->setEndValue(targetHeight);
    expandAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(expandAnimation, &QPropertyAnimation::finished, this, [this, expandAnimation]() {
        if (m_expandAnimation == expandAnimation && view()) {
            view()->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        expandAnimation->deleteLater();
    });
    expandAnimation->start();
}

void AnimatedComboBox::hidePopup()
{
    if (m_expandAnimation) {
        m_expandAnimation->stop();
        m_expandAnimation->deleteLater();
        m_expandAnimation = nullptr;
    }
    QComboBox::hidePopup();
    if (view()) {
        view()->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}
