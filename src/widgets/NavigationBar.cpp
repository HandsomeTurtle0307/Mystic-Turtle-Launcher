/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "NavigationBar.h"

#include "IconFactory.h"
#include "NavButton.h"
#include "Theme.h"

#include <QAbstractAnimation>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>

namespace {

QRectF interpolateRect(const QRectF &from, const QRectF &to, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QRectF(from.x() + (to.x() - from.x()) * t,
                  from.y() + (to.y() - from.y()) * t,
                  from.width() + (to.width() - from.width()) * t,
                  from.height() + (to.height() - from.height()) * t);
}

} // namespace

NavigationBar::NavigationBar(QWidget *parent)
    : QWidget(parent)
    , m_indicatorAnimation(new QVariantAnimation(this))
{
    setFixedHeight(46);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    const struct {
        const char *text;
        Icons::Type icon;
    } items[] = {
        {"启动", Icons::Type::Play},
        {"下载游戏", Icons::Type::Gamepad},
        {"下载模组", Icons::Type::Cube},
        {"设置", Icons::Type::Sliders},
        {"关于", Icons::Type::Info},
    };

    int index = 0;
    for (const auto &item : items) {
        auto *button = new NavButton(QString::fromUtf8(item.text), item.icon, this);
        button->setAccessibleName(QString::fromUtf8(item.text));
        layout->addWidget(button);
        m_buttons.append(button);

        connect(button, &QAbstractButton::clicked, this, [this, index]() {
            if (m_currentIndex == index) {
                return;
            }
            setCurrentIndex(index);
            emit navigationRequested(index);
        });
        ++index;
    }

    m_buttons.first()->setActive(true, false);

    m_indicatorAnimation->setDuration(300);
    m_indicatorAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_indicatorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_indicatorRect = interpolateRect(m_indicatorFrom, m_indicatorTo, value.toReal());
                update();
            });

    QTimer::singleShot(0, this, [this]() { updateIndicator(false); });
}

void NavigationBar::setCurrentIndex(int index, bool animated)
{
    if (index < 0 || index >= m_buttons.size()) {
        return;
    }
    if (!m_buttons.at(index)->isEnabled()) {
        return;
    }

    const bool changed = m_currentIndex != index;
    m_currentIndex = index;
    for (int i = 0; i < m_buttons.size(); ++i) {
        m_buttons.at(i)->setActive(i == index, animated);
    }

    if (changed || !m_indicatorInitialized) {
        updateIndicator(animated);
    }
}

int NavigationBar::currentIndex() const
{
    return m_currentIndex;
}

void NavigationBar::setItemEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_buttons.size()) {
        return;
    }

    NavButton *button = m_buttons.at(index);
    button->setEnabled(enabled);
    button->setToolTip(enabled ? QString() : QStringLiteral("即将开放"));
    if (!enabled) {
        button->setActive(false, false);
    }
}

bool NavigationBar::isItemEnabled(int index) const
{
    if (index < 0 || index >= m_buttons.size()) {
        return false;
    }
    return m_buttons.at(index)->isEnabled();
}

void NavigationBar::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (!enabled) {
        m_indicatorAnimation->stop();
        if (m_indicatorInitialized) {
            m_indicatorRect = targetIndicatorRect(m_currentIndex);
            update();
        }
    }
}

bool NavigationBar::animationsEnabled() const
{
    return m_animationsEnabled;
}

QRectF NavigationBar::targetIndicatorRect(int index) const
{
    if (index < 0 || index >= m_buttons.size()) {
        return QRectF();
    }
    return QRectF(m_buttons.at(index)->geometry());
}

void NavigationBar::updateIndicator(bool animated)
{
    const QRectF target = targetIndicatorRect(m_currentIndex);
    if (!target.isValid()) {
        return;
    }

    if (!m_indicatorInitialized) {
        m_indicatorAnimation->stop();
        m_indicatorRect = target;
        m_indicatorFrom = target;
        m_indicatorTo = target;
        m_indicatorInitialized = true;
        update();
        return;
    }

    m_indicatorAnimation->stop();
    m_indicatorFrom = m_indicatorRect;
    m_indicatorTo = target;

    if (!animated || !m_animationsEnabled || !Theme::animationsEnabled() || !isVisible()) {
        m_indicatorRect = target;
        update();
        return;
    }

    m_indicatorAnimation->setStartValue(0.0);
    m_indicatorAnimation->setEndValue(1.0);
    m_indicatorAnimation->start();
}

void NavigationBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    Q_UNUSED(bounds)
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::NoBrush);

    if (m_indicatorInitialized && m_indicatorRect.isValid() && m_indicatorRect.width() > 0) {
        const QRectF pill = m_indicatorRect.adjusted(0.5, 0.5, -0.5, -0.5);
        if (Theme::isDarkMode()) {
            painter.setBrush(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                    Theme::Accent.blue(), 30));
            painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                       Theme::Accent.blue(), 80), 1.0));
        } else {
            painter.setBrush(QColor(255, 255, 255, 246));
            painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
        }
        painter.drawRoundedRect(pill, 10, 10);
    }
}

void NavigationBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_indicatorAnimation->state() == QAbstractAnimation::Running) {
        m_indicatorAnimation->stop();
    }
    const QRectF target = targetIndicatorRect(m_currentIndex);
    if (target.isValid()) {
        m_indicatorRect = target;
        m_indicatorInitialized = true;
        update();
    }
}

void NavigationBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { updateIndicator(false); });
}
