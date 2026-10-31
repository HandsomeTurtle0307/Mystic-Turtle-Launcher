/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "LogoMark.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {

void paintLogo(QPainter &painter, const QRectF &bounds)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal size = qMin(bounds.width(), bounds.height());
    QRectF tile(0, 0, size, size);
    tile.moveCenter(bounds.center());
    const qreal radius = size * 0.29;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 55));
    painter.drawRoundedRect(tile.translated(0, size * 0.045), radius, radius);

    QLinearGradient tileGradient(tile.topLeft(), tile.bottomRight());
    tileGradient.setColorAt(0.0, QColor(QStringLiteral("#8CF0C8")));
    tileGradient.setColorAt(0.48, QColor(QStringLiteral("#76B6F2")));
    tileGradient.setColorAt(1.0, QColor(QStringLiteral("#A68BF5")));
    painter.setBrush(tileGradient);
    painter.drawRoundedRect(tile, radius, radius);

    QLinearGradient highlight(tile.topLeft(), tile.bottomLeft());
    highlight.setColorAt(0.0, QColor(255, 255, 255, 75));
    highlight.setColorAt(0.55, QColor(255, 255, 255, 0));
    painter.setBrush(highlight);
    painter.drawRoundedRect(tile.adjusted(1, 1, -1, -1), radius - 1, radius - 1);

    painter.setPen(QPen(QColor(255, 255, 255, 42), qMax(1.0, size * 0.022)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(tile.adjusted(size * 0.02, size * 0.02, -size * 0.02, -size * 0.02),
                            radius - 1, radius - 1);

    const QPointF center(tile.center().x(), tile.center().y() + size * 0.035);
    const qreal shellW = size * 0.30;
    const qreal shellH = size * 0.245;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(5, 38, 33, 190));
    painter.drawEllipse(center + QPointF(size * 0.265, -size * 0.01),
                        size * 0.078, size * 0.078);
    painter.drawEllipse(center + QPointF(-size * 0.31, -size * 0.15),
                        size * 0.07, size * 0.052);
    painter.drawEllipse(center + QPointF(-size * 0.30, size * 0.14),
                        size * 0.075, size * 0.055);
    painter.drawEllipse(center + QPointF(size * 0.17, size * 0.225),
                        size * 0.078, size * 0.052);

    painter.setBrush(QColor(6, 44, 38, 235));
    painter.drawEllipse(center, shellW, shellH);

    QPen shellPen(QColor(142, 245, 203, 225), qMax(1.2, size * 0.034));
    shellPen.setCapStyle(Qt::RoundCap);
    painter.setPen(shellPen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath spiral;
    spiral.moveTo(center + QPointF(-size * 0.125, size * 0.015));
    spiral.cubicTo(center + QPointF(-size * 0.10, -size * 0.12),
                   center + QPointF(size * 0.12, -size * 0.13),
                   center + QPointF(size * 0.14, size * 0.01));
    spiral.cubicTo(center + QPointF(size * 0.16, size * 0.13),
                   center + QPointF(-size * 0.05, size * 0.16),
                   center + QPointF(-size * 0.06, size * 0.04));
    painter.drawPath(spiral);

    painter.setPen(QPen(QColor(255, 255, 255, 130), qMax(1.0, size * 0.018)));
    painter.drawArc(QRectF(center.x() - shellW, center.y() - shellH,
                           shellW * 1.4, shellH * 1.15),
                    -25 * 16, 70 * 16);

    painter.restore();
}

} // namespace

LogoMark::LogoMark(int size, QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(size, size);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
}

QIcon LogoMark::appIcon()
{
    QIcon result;
    const int sizes[] = {16, 20, 24, 32, 48, 64, 128, 256};
    for (int size : sizes) {
        QPixmap pixmap(size * 2, size * 2);
        pixmap.fill(Qt::transparent);
        pixmap.setDevicePixelRatio(2.0);
        QPainter painter(&pixmap);
        paintLogo(painter, QRectF(0, 0, size, size));
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}

void LogoMark::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    paintLogo(painter, QRectF(rect()));
}
