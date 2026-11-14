/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "HeaderBar.h"

#include "AvatarWidget.h"
#include "IconFactory.h"
#include "LogoMark.h"
#include "NavigationBar.h"
#include "Theme.h"
#include "WindowButton.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QWindow>

HeaderBar::HeaderBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 10, 12, 10);
    layout->setSpacing(10);

    auto *brand = new QWidget(this);
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    brand->setMinimumWidth(230);
    brand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(10);

    brandLayout->addWidget(new LogoMark(38, brand));
    auto *brandText = new QVBoxLayout();
    brandText->setContentsMargins(0, 0, 0, 0);
    brandText->setSpacing(1);
    auto *title = Theme::label(QStringLiteral("MysticTurtle"), 17, QFont::DemiBold,
                               QColor(QStringLiteral("#FFFFFF")), brand, true);
    brandText->addWidget(title);
    brandLayout->addLayout(brandText);
    brandLayout->addStretch();

    m_navigationBar = new NavigationBar(this);

    auto *right = new QWidget(this);
    right->setMinimumWidth(350);
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *rightLayout = new QHBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    rightLayout->addStretch();
    rightLayout->addSpacing(4);

    m_accountButton = new QPushButton(right);
    m_accountButton->setCursor(Qt::PointingHandCursor);
    m_accountButton->setFlat(true);
    m_accountButton->setAccessibleName(QStringLiteral("切换账户"));
    m_accountButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid transparent;"
        "border-radius: 10px; text-align: left; padding: 3px 7px; }"
        "QPushButton:hover { background: rgba(255,255,255,10);"
        "border-color: rgba(255,255,255,20); }"));
    auto *accountLayout = new QHBoxLayout(m_accountButton);
    accountLayout->setContentsMargins(0, 0, 0, 0);
    accountLayout->setSpacing(7);
    m_accountAvatar = new AvatarWidget(28, m_accountButton);
    m_accountAvatar->setOnline(false);
    accountLayout->addWidget(m_accountAvatar, 0, Qt::AlignVCenter);
    auto *accountText = new QVBoxLayout();
    accountText->setContentsMargins(0, 0, 0, 0);
    accountText->setSpacing(0);
    m_accountName = Theme::label(QStringLiteral("未添加账户"), 10, QFont::DemiBold,
                                 Theme::TextPrimary, m_accountButton);
    m_accountName->setMaximumWidth(104);
    m_accountType = Theme::label(QString(), 8, QFont::Normal,
                                 Theme::TextMuted, m_accountButton);
    accountText->addWidget(m_accountName);
    accountText->addWidget(m_accountType);
    accountLayout->addLayout(accountText, 1);
    connect(m_accountButton, &QAbstractButton::clicked,
            this, &HeaderBar::accountRequested);
    rightLayout->addWidget(m_accountButton, 0, Qt::AlignVCenter);
    rightLayout->addSpacing(6);

    auto *windowControls = new QWidget(right);
    auto *controlsLayout = new QHBoxLayout(windowControls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(1);

    m_minimizeButton = new WindowButton(Icons::Type::Minimize, false, windowControls);
    m_minimizeButton->setToolTip(QStringLiteral("最小化"));
    m_minimizeButton->setAccessibleName(QStringLiteral("最小化"));
    m_maximizeButton = new WindowButton(Icons::Type::Maximize, false, windowControls);
    m_maximizeButton->setToolTip(QStringLiteral("最大化"));
    m_maximizeButton->setAccessibleName(QStringLiteral("最大化"));
    m_closeButton = new WindowButton(Icons::Type::Close, true, windowControls);
    m_closeButton->setToolTip(QStringLiteral("关闭"));
    m_closeButton->setAccessibleName(QStringLiteral("关闭"));

    controlsLayout->addWidget(m_minimizeButton);
    controlsLayout->addWidget(m_maximizeButton);
    controlsLayout->addWidget(m_closeButton);
    rightLayout->addWidget(windowControls);

    layout->addWidget(brand, 1);
    layout->addSpacing(6);
    layout->addWidget(m_navigationBar, 0, Qt::AlignCenter);
    layout->addSpacing(6);
    layout->addWidget(right, 1);

    connect(m_minimizeButton, &QAbstractButton::clicked, this, &HeaderBar::minimizeRequested);
    connect(m_maximizeButton, &QAbstractButton::clicked, this, &HeaderBar::maximizeRestoreRequested);
    connect(m_closeButton, &QAbstractButton::clicked, this, &HeaderBar::closeRequested);
}

NavigationBar *HeaderBar::navigationBar() const
{
    return m_navigationBar;
}

void HeaderBar::setAccount(const QString &name, const QString &typeText)
{
    if (!m_accountName || !m_accountType) {
        return;
    }
    const QString displayName = name.trimmed().isEmpty()
        ? QStringLiteral("未添加账户")
        : name.trimmed();
    const QFontMetrics metrics(m_accountName->font());
    m_accountName->setText(metrics.elidedText(displayName, Qt::ElideRight, 104));
    m_accountName->setToolTip(displayName);
    m_accountType->setText(typeText);
    m_accountType->setVisible(!typeText.trimmed().isEmpty());
}

void HeaderBar::setAccountName(const QString &name)
{
    setAccount(name, m_accountType ? m_accountType->text() : QString());
}

void HeaderBar::setMaximized(bool maximized)
{
    m_maximizeButton->setIconType(maximized ? Icons::Type::Restore : Icons::Type::Maximize);
    m_maximizeButton->setToolTip(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
    m_maximizeButton->setAccessibleName(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
}

void HeaderBar::startWindowMove()
{
    if (window() && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
    }
}

void HeaderBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        startWindowMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void HeaderBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeRestoreRequested();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void HeaderBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    if (Theme::liquidGlassEnabled()) {
        QLinearGradient glass(0, 0, 0, height());
        glass.setColorAt(0.0, QColor(255, 255, 255,
                                     Theme::isDarkMode() ? 13 : 46));
        glass.setColorAt(0.72, QColor(255, 255, 255,
                                      Theme::isDarkMode() ? 2 : 12));
        glass.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(rect(), glass);
        QRadialGradient bloom(QPointF(width() * 0.24, -16), qMax(180.0, width() * 0.5));
        bloom.setColorAt(0.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(), 20));
        bloom.setColorAt(1.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                     Theme::Accent.blue(), 0));
        painter.fillRect(rect(), bloom);
    }
    painter.setPen(QPen(QColor(255, 255, 255, Theme::isDarkMode() ? 16 : 26), 1.0));
    painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}
