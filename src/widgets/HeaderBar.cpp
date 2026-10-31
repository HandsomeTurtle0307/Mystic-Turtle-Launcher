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
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class UserChip : public QAbstractButton
{
public:
    explicit UserChip(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setFixedHeight(40);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setToolTip(QStringLiteral("管理本地账户"));
        setAccessibleName(QStringLiteral("管理本地账户"));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 5, 12, 5);
        layout->setSpacing(8);

        auto *avatar = new AvatarWidget(30, this);
        avatar->setOnline(false);
        layout->addWidget(avatar);

        m_name = Theme::label(QStringLiteral("MysticTurtle"), 12, QFont::DemiBold,
                              QColor(QStringLiteral("#FFFFFF")), this);
        layout->addWidget(m_name);
    }

    QLabel *nameLabel() const
    {
        return m_name;
    }

    QSize sizeHint() const override
    {
        return layout() ? layout()->sizeHint() : QSize(120, 40);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const int alpha = isDown() ? 34 : (underMouse() ? 26 : 16);
        const int borderAlpha = isDown() ? 110 : (underMouse() ? 90 : 62);
        painter.setPen(QPen(QColor(255, 255, 255, borderAlpha), 1.0));
        painter.setBrush(QColor(255, 255, 255, alpha));
        painter.drawRoundedRect(bounds, 13, 13);
    }

    void enterEvent(QEnterEvent *event) override
    {
        QAbstractButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QAbstractButton::leaveEvent(event);
        update();
    }

private:
    QLabel *m_name = nullptr;
};

} // namespace

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
    right->setMinimumWidth(230);
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *rightLayout = new QHBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    rightLayout->addStretch();
    rightLayout->addSpacing(4);

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

void HeaderBar::setMaximized(bool maximized)
{
    m_maximizeButton->setIconType(maximized ? Icons::Type::Restore : Icons::Type::Maximize);
    m_maximizeButton->setToolTip(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
    m_maximizeButton->setAccessibleName(maximized ? QStringLiteral("还原") : QStringLiteral("最大化"));
}

void HeaderBar::setAccountName(const QString &username)
{
    if (!m_userNameLabel) {
        return;
    }
    const QString displayName = username.trimmed();
    m_userNameLabel->setText(displayName.isEmpty() ? QStringLiteral("未添加账户")
                                                   : displayName);
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
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient background(0, 0, width(), height());
    if (Theme::isDarkMode()) {
        background.setColorAt(0.0, QColor(QStringLiteral("#17212C")));
        background.setColorAt(0.55, QColor(QStringLiteral("#111821")));
        background.setColorAt(1.0, QColor(QStringLiteral("#0D1117")));
    } else {
        background.setColorAt(0.0, QColor(QStringLiteral("#2F80ED")));
        background.setColorAt(0.55, QColor(QStringLiteral("#2878E8")));
        background.setColorAt(1.0, QColor(QStringLiteral("#1E6FE0")));
    }

    painter.fillRect(rect(), background);

    painter.setPen(QPen(QColor(255, 255, 255, Theme::isDarkMode() ? 16 : 26), 1.0));
    painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}
