/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AnimatedStackedWidget.h"

#include "Theme.h"

#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QResizeEvent>

AnimatedStackedWidget::AnimatedStackedWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void AnimatedStackedWidget::addPage(QWidget *page)
{
    if (!page) {
        return;
    }

    page->setParent(this);
    page->setGeometry(rect());
    page->hide();
    m_pages.append(page);

    if (m_pages.size() == 1) {
        m_currentIndex = 0;
        page->show();
    }
}

void AnimatedStackedWidget::setCurrentIndex(int index, bool animated)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }

    if (m_animation) {
        finishTransition();
    }

    if (index == m_currentIndex) {
        if (QWidget *page = currentPage()) {
            page->show();
            page->raise();
        }
        return;
    }

    animateTo(index, animated);
}

int AnimatedStackedWidget::currentIndex() const
{
    return m_currentIndex;
}

QWidget *AnimatedStackedWidget::currentPage() const
{
    return (m_currentIndex >= 0 && m_currentIndex < m_pages.size())
        ? m_pages.at(m_currentIndex)
        : nullptr;
}

int AnimatedStackedWidget::count() const
{
    return m_pages.size();
}

void AnimatedStackedWidget::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (!enabled && m_animation) {
        finishTransition();
    }
}

bool AnimatedStackedWidget::animationsEnabled() const
{
    return m_animationsEnabled;
}

void AnimatedStackedWidget::animateTo(int index, bool animated)
{
    QWidget *outgoing = currentPage();
    QWidget *incoming = m_pages.at(index);
    const int previousIndex = m_currentIndex;
    const int direction = index > previousIndex ? 1 : -1;

    m_currentIndex = index;
    emit currentChanged(index);

    for (QWidget *page : m_pages) {
        if (page != outgoing && page != incoming) {
            page->hide();
        }
    }

    incoming->setGeometry(rect());
    incoming->show();
    incoming->raise();

    if (!animated || !m_animationsEnabled || !Theme::animationsEnabled()
        || !isVisible() || !outgoing) {
        if (outgoing && outgoing != incoming) {
            outgoing->hide();
            outgoing->setGeometry(rect());
        }
        incoming->setGeometry(rect());
        incoming->raise();
        return;
    }

    m_outgoing = outgoing;
    m_incoming = incoming;

    auto *oldEffect = new QGraphicsOpacityEffect(outgoing);
    oldEffect->setOpacity(1.0);
    outgoing->setGraphicsEffect(oldEffect);

    auto *newEffect = new QGraphicsOpacityEffect(incoming);
    newEffect->setOpacity(0.0);
    incoming->setGraphicsEffect(newEffect);

    incoming->move(direction * 28, 0);
    outgoing->move(0, 0);

    m_animation = new QParallelAnimationGroup(this);

    auto *oldPosition = new QPropertyAnimation(outgoing, "pos", m_animation);
    oldPosition->setDuration(260);
    oldPosition->setStartValue(QPoint(0, 0));
    oldPosition->setEndValue(QPoint(-direction * 18, 0));
    oldPosition->setEasingCurve(QEasingCurve::OutCubic);

    auto *newPosition = new QPropertyAnimation(incoming, "pos", m_animation);
    newPosition->setDuration(320);
    newPosition->setStartValue(QPoint(direction * 28, 0));
    newPosition->setEndValue(QPoint(0, 0));
    newPosition->setEasingCurve(QEasingCurve::OutCubic);

    auto *oldOpacity = new QPropertyAnimation(oldEffect, "opacity", m_animation);
    oldOpacity->setDuration(180);
    oldOpacity->setStartValue(1.0);
    oldOpacity->setEndValue(0.0);
    oldOpacity->setEasingCurve(QEasingCurve::InOutQuad);

    auto *newOpacity = new QPropertyAnimation(newEffect, "opacity", m_animation);
    newOpacity->setDuration(250);
    newOpacity->setStartValue(0.0);
    newOpacity->setEndValue(1.0);
    newOpacity->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_animation, &QParallelAnimationGroup::finished,
            this, &AnimatedStackedWidget::finishTransition);
    m_animation->start();
}

void AnimatedStackedWidget::finishTransition()
{
    if (m_animation) {
        m_animation->stop();
        m_animation->deleteLater();
        m_animation = nullptr;
    }

    if (m_outgoing) {
        m_outgoing->hide();
        m_outgoing->setGraphicsEffect(nullptr);
        m_outgoing->setGeometry(rect());
        m_outgoing = nullptr;
    }

    if (m_incoming) {
        m_incoming->setGraphicsEffect(nullptr);
        m_incoming->setGeometry(rect());
        m_incoming->show();
        m_incoming->raise();
        m_incoming = nullptr;
    }

    if (QWidget *page = currentPage()) {
        page->setGraphicsEffect(nullptr);
        page->setGeometry(rect());
        page->show();
        page->raise();
    }

    for (QWidget *page : m_pages) {
        if (page != currentPage()) {
            page->hide();
        }
    }
}

void AnimatedStackedWidget::cancelTransition()
{
    finishTransition();
}

void AnimatedStackedWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_animation) {
        finishTransition();
    }

    for (QWidget *page : m_pages) {
        page->setGeometry(rect());
    }
}
