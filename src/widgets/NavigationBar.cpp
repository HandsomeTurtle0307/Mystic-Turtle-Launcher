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
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
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
    , m_spotlightAnimation(new QVariantAnimation(this))
{
    setFixedHeight(46);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(0);

    const struct {
        const char *text;
        Icons::Type icon;
    } items[] = {
        {"启动", Icons::Type::Play},
        {"下载", Icons::Type::Download},
        {"设置", Icons::Type::Sliders},
        {"联机", Icons::Type::External},
        {"更多", Icons::Type::Boxes},
    };

    int index = 0;
    for (const auto &item : items) {
        auto *button = new NavButton(QString::fromUtf8(item.text), item.icon, this);
        button->setAccessibleName(QString::fromUtf8(item.text));
        button->installEventFilter(this);
        layout->addWidget(button);
        m_buttons.append(button);

        connect(button, &QAbstractButton::clicked, this, [this, index]() {
            if (m_currentIndex != index) {
                setCurrentIndex(index);
            }
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

    m_spotlightAnimation->setDuration(230);
    m_spotlightAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_spotlightAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_spotlightProgress = qBound(0.0, value.toReal(), 1.0);
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

bool NavigationBar::eventFilter(QObject *watched, QEvent *event)
{
    auto *button = qobject_cast<NavButton *>(watched);
    if (button) {
        switch (event->type()) {
        case QEvent::Enter:
            updateSpotlightPosition(QCursor::pos());
            if (Theme::buttonSpotlightEnabled()) {
                animateSpotlight(1.0);
            }
            break;
        case QEvent::MouseMove:
            updateSpotlightPosition(
                static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
            break;
        case QEvent::Leave:
            QTimer::singleShot(0, this, [this]() {
                if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                    animateSpotlight(0.0);
                }
            });
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NavigationBar::updateSpotlightPosition(const QPoint &globalPosition)
{
    m_spotlightPosition = mapFromGlobal(globalPosition);
    update();
}

void NavigationBar::animateSpotlight(qreal target)
{
    m_spotlightAnimation->stop();
    if (!m_animationsEnabled || !Theme::animationsEnabled()) {
        m_spotlightProgress = target;
        update();
        return;
    }
    m_spotlightAnimation->setStartValue(m_spotlightProgress);
    m_spotlightAnimation->setEndValue(target);
    m_spotlightAnimation->start();
}

void NavigationBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = height() / 2.0;
    QPainterPath capsule;
    capsule.addRoundedRect(bounds, radius, radius);

    painter.save();
    painter.setClipPath(capsule);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, Theme::isDarkMode() ? 9 : 34));
    painter.drawRoundedRect(bounds, radius, radius);

    if (Theme::liquidGlassEnabled()) {
        QLinearGradient glass(bounds.topLeft(), bounds.bottomLeft());
        glass.setColorAt(0.0, QColor(255, 255, 255,
                                     Theme::isDarkMode() ? 20 : 54));
        glass.setColorAt(0.44, QColor(255, 255, 255,
                                      Theme::isDarkMode() ? 4 : 18));
        glass.setColorAt(1.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(),
                                     Theme::isDarkMode() ? 12 : 20));
        painter.fillRect(bounds, glass);

        QRadialGradient bloom(QPointF(width() * 0.26, -10.0),
                              qMax(100.0, width() * 0.52));
        bloom.setColorAt(0.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(), 26));
        bloom.setColorAt(1.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(), 0));
        painter.fillRect(bounds, bloom);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255,
                                   Theme::isDarkMode() ? 34 : 82), 1.0));
        painter.drawLine(QPointF(bounds.left() + radius, bounds.top() + 1.1),
                         QPointF(bounds.right() - radius, bounds.top() + 1.1));
    }

    if (Theme::buttonSpotlightEnabled() && m_spotlightProgress > 0.001) {
        const qreal lightRadius = qMax(100.0, width() * 0.78);
        QRadialGradient light(m_spotlightPosition, lightRadius);
        light.setColorAt(0.0, QColor(255, 255, 255,
                                     static_cast<int>(38 * m_spotlightProgress)));
        light.setColorAt(0.35, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                      Theme::Accent.blue(),
                                      static_cast<int>(18 * m_spotlightProgress)));
        light.setColorAt(0.7, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(),
                                     static_cast<int>(5 * m_spotlightProgress)));
        light.setColorAt(1.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.fillRect(bounds, light);
    }
    painter.restore();

    if (m_indicatorInitialized && m_indicatorRect.isValid() && m_indicatorRect.width() > 0) {
        const QRectF pill = m_indicatorRect.adjusted(1.0, 1.0, -1.0, -1.0);
        QColor pillColor = Theme::Accent;
        pillColor.setAlpha(224);
        painter.setBrush(pillColor);
        painter.setPen(QPen(QColor(Theme::Accent.red(), Theme::Accent.green(),
                                   Theme::Accent.blue(), 242), 1.0));
        painter.drawRoundedRect(pill, height() / 2.0 - 2.0, height() / 2.0 - 2.0);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255,
                               Theme::isDarkMode() ? 28 : 78), 1.0));
    painter.drawRoundedRect(bounds, radius, radius);
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
