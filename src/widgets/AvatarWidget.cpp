/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AvatarWidget.h"

#include "Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

AvatarWidget::AvatarWidget(int diameter, QWidget *parent)
    : QWidget(parent)
    , m_diameter(diameter)
{
    setFixedSize(diameter, diameter);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
}

void AvatarWidget::setOnline(bool online)
{
    if (m_online == online) {
        return;
    }
    m_online = online;
    update();
}

bool AvatarWidget::isOnline() const
{
    return m_online;
}

void AvatarWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal size = qMin(width(), height());
    const QRectF outer(0.5, 0.5, size - 1.0, size - 1.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 60));
    painter.drawEllipse(outer.translated(0, 1.5));

    QLinearGradient ring(outer.topLeft(), outer.bottomRight());
    ring.setColorAt(0.0, QColor(QStringLiteral("#7BE9BF")));
    ring.setColorAt(0.55, QColor(QStringLiteral("#7DA9F5")));
    ring.setColorAt(1.0, QColor(QStringLiteral("#A58AF0")));
    painter.setBrush(ring);
    painter.drawEllipse(outer);

    const QRectF face = outer.adjusted(2.0, 2.0, -2.0, -2.0);
    QPainterPath clip;
    clip.addEllipse(face);
    painter.save();
    painter.setClipPath(clip);

    QLinearGradient backdrop(face.topLeft(), face.bottomRight());
    backdrop.setColorAt(0.0, Theme::isDarkMode()
                                 ? QColor(QStringLiteral("#1D3550"))
                                 : QColor(QStringLiteral("#EAF4FF")));
    backdrop.setColorAt(0.48, Theme::isDarkMode()
                                  ? QColor(QStringLiteral("#172A40"))
                                  : QColor(QStringLiteral("#D6E9FF")));
    backdrop.setColorAt(1.0, Theme::isDarkMode()
                                 ? QColor(QStringLiteral("#101C2B"))
                                 : QColor(QStringLiteral("#C2DDFB")));
    painter.fillRect(face, backdrop);

    const qreal unit = face.width() / 16.0;
    painter.setPen(QPen(QColor(47, 128, 237, 40), 1.0));
    for (int i = 1; i < 16; ++i) {
        painter.drawLine(QPointF(face.left() + i * unit, face.top()),
                         QPointF(face.left() + i * unit, face.bottom()));
        painter.drawLine(QPointF(face.left(), face.top() + i * unit),
                         QPointF(face.right(), face.top() + i * unit));
    }

    QRadialGradient glow(face.center() - QPointF(unit * 2.5, unit * 3.5), face.width() * 0.65);
    glow.setColorAt(0.0, QColor(47, 128, 237, 55));
    glow.setColorAt(1.0, QColor(47, 128, 237, 0));
    painter.fillRect(face, glow);

    auto cell = [&](int x, int y, int w, int h, const QColor &color) {
        painter.fillRect(QRectF(face.left() + x * unit, face.top() + y * unit,
                                w * unit + 0.2, h * unit + 0.2),
                         color);
    };

    const QColor starGold(QStringLiteral("#F4DA93"));
    const QColor starSoft(QStringLiteral("#C7B6FF"));
    cell(2, 2, 1, 1, starGold);
    cell(13, 4, 1, 1, starSoft);
    cell(12, 2, 1, 1, QColor(47, 128, 237, 55));
    cell(3, 12, 1, 1, QColor(47, 128, 237, 40));

    const QColor hatTip(QStringLiteral("#8F76D8"));
    const QColor hatMid(QStringLiteral("#6D55B6"));
    const QColor hatDark(QStringLiteral("#4C3B86"));
    const QColor hatBrim(QStringLiteral("#392C68"));
    cell(8, 1, 1, 1, hatTip);
    cell(7, 2, 3, 1, hatTip);
    cell(6, 3, 5, 1, hatMid);
    cell(5, 4, 7, 1, hatMid);
    cell(4, 5, 9, 1, hatDark);
    cell(3, 6, 11, 1, hatBrim);
    cell(4, 7, 9, 1, hatDark);
    cell(9, 3, 1, 1, starGold);

    const QColor skin(QStringLiteral("#67CE9C"));
    const QColor skinLight(QStringLiteral("#8BE7B5"));
    const QColor skinDark(QStringLiteral("#3E9F75"));
    const QColor skinShadow(QStringLiteral("#2D7E5B"));
    cell(4, 8, 9, 1, skin);
    cell(4, 7, 1, 1, skinLight);
    cell(12, 7, 1, 1, skinDark);
    cell(3, 8, 12, 1, skin);
    cell(3, 9, 1, 2, skinDark);
    cell(13, 9, 1, 2, skinDark);
    cell(4, 9, 9, 3, skin);
    cell(5, 9, 2, 1, skinLight);
    cell(10, 9, 3, 1, skin);
    cell(4, 11, 9, 1, skinDark);

    cell(5, 9, 1, 1, QColor(QStringLiteral("#EAF8F0")));
    cell(6, 9, 1, 1, QColor(QStringLiteral("#0D1A16")));
    cell(10, 9, 1, 1, QColor(QStringLiteral("#0D1A16")));
    cell(11, 9, 1, 1, QColor(QStringLiteral("#EAF8F0")));
    cell(7, 10, 3, 1, skinShadow);
    cell(8, 10, 1, 1, QColor(QStringLiteral("#255F49")));
    cell(9, 11, 1, 1, skinLight);

    const QColor shell(QStringLiteral("#356E5D"));
    const QColor shellDark(QStringLiteral("#225446"));
    const QColor shellLight(QStringLiteral("#63B48C"));
    cell(6, 12, 5, 1, skinDark);
    cell(4, 13, 9, 1, shellDark);
    cell(3, 14, 11, 2, shell);
    cell(2, 14, 1, 1, skinDark);
    cell(14, 14, 1, 1, skinDark);
    cell(4, 13, 2, 1, shellLight);
    cell(10, 13, 2, 1, shellDark);
    cell(5, 14, 2, 1, QColor(QStringLiteral("#4E9978")));
    cell(8, 14, 2, 1, shellLight);
    cell(11, 14, 2, 1, QColor(QStringLiteral("#4E9978")));
    cell(4, 15, 1, 1, shellDark);
    cell(8, 15, 2, 1, QColor(QStringLiteral("#2A604F")));
    cell(12, 15, 1, 1, shellDark);

    QLinearGradient vignette(face.topLeft(), face.bottomLeft());
    vignette.setColorAt(0.0, QColor(23, 36, 58, 0));
    vignette.setColorAt(0.64, QColor(23, 36, 58, 0));
    vignette.setColorAt(1.0, QColor(23, 36, 58, 24));
    painter.fillRect(face, vignette);

    painter.restore();

    painter.setPen(QPen(QColor(23, 36, 58, 55), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(face.adjusted(0.5, 0.5, -0.5, -0.5));

    if (m_online && m_diameter >= 40) {
        const qreal dotRadius = qMax(4.0, size * 0.075);
        const QPointF dotCenter(face.right() - dotRadius * 0.8,
                                face.bottom() - dotRadius * 0.8);
        painter.setPen(QPen(QColor(12, 18, 24), qMax(2.0, size * 0.035)));
        painter.setBrush(QColor(QStringLiteral("#54E3A4")));
        painter.drawEllipse(dotCenter, dotRadius, dotRadius);
    }
}
