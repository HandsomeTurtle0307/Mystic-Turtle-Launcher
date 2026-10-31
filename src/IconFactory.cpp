/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "IconFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

namespace {

void drawStar(QPainter &painter, const QPointF &center, qreal radius)
{
    QPainterPath path;
    path.moveTo(center.x(), center.y() - radius);
    path.cubicTo(center.x() + radius * 0.16, center.y() - radius * 0.18,
                 center.x() + radius * 0.18, center.y() - radius * 0.16,
                 center.x() + radius, center.y());
    path.cubicTo(center.x() + radius * 0.18, center.y() + radius * 0.16,
                 center.x() + radius * 0.16, center.y() + radius * 0.18,
                 center.x(), center.y() + radius);
    path.cubicTo(center.x() - radius * 0.16, center.y() + radius * 0.18,
                 center.x() - radius * 0.18, center.y() + radius * 0.16,
                 center.x() - radius, center.y());
    path.cubicTo(center.x() - radius * 0.18, center.y() - radius * 0.16,
                 center.x() - radius * 0.16, center.y() - radius * 0.18,
                 center.x(), center.y() - radius);
    painter.drawPath(path);
}

} // namespace

namespace Icons {

QIcon icon(Type type, const QColor &color, int size)
{
    constexpr int scale = 2;
    QPixmap pixmap(size * scale, size * scale);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paint(painter, type, QRectF(0, 0, size, size), color, 1.7);
    painter.end();

    return QIcon(pixmap);
}

void paint(QPainter &painter,
           Type type,
           const QRectF &rect,
           const QColor &color,
           qreal strokeWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.topLeft());
    painter.scale(rect.width() / 24.0, rect.height() / 24.0);

    QPen pen(color, strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (type) {
    case Type::Home: {
        QPainterPath roof;
        roof.moveTo(3.8, 10.5);
        roof.lineTo(12.0, 3.8);
        roof.lineTo(20.2, 10.5);
        painter.drawPath(roof);

        QPainterPath house;
        house.moveTo(6.1, 9.2);
        house.lineTo(6.1, 20.1);
        house.lineTo(17.9, 20.1);
        house.lineTo(17.9, 9.2);
        painter.drawPath(house);

        painter.drawRoundedRect(QRectF(9.7, 14.2, 4.6, 5.9), 1.2, 1.2);
        break;
    }
    case Type::Gamepad: {
        painter.drawRoundedRect(QRectF(2.8, 6.9, 18.4, 11.6), 5.0, 5.0);
        painter.drawLine(QPointF(8.1, 10.6), QPointF(8.1, 14.4));
        painter.drawLine(QPointF(6.2, 12.5), QPointF(10.0, 12.5));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(16.0, 11.3), 0.9, 0.9);
        painter.drawEllipse(QPointF(18.2, 13.7), 0.9, 0.9);
        break;
    }
    case Type::Cube:
    case Type::Package: {
        QPainterPath top;
        top.moveTo(12.0, 3.2);
        top.lineTo(20.0, 7.4);
        top.lineTo(12.0, 11.6);
        top.lineTo(4.0, 7.4);
        top.closeSubpath();
        painter.drawPath(top);

        QPainterPath body;
        body.moveTo(4.0, 7.4);
        body.lineTo(4.0, 16.3);
        body.lineTo(12.0, 20.5);
        body.lineTo(12.0, 11.6);
        body.moveTo(20.0, 7.4);
        body.lineTo(20.0, 16.3);
        body.lineTo(12.0, 20.5);
        painter.drawPath(body);

        if (type == Type::Package) {
            painter.drawLine(QPointF(8.0, 5.3), QPointF(16.0, 9.5));
        }
        break;
    }
    case Type::Sliders: {
        painter.drawLine(QPointF(4.2, 6.2), QPointF(19.8, 6.2));
        painter.drawLine(QPointF(4.2, 12.0), QPointF(19.8, 12.0));
        painter.drawLine(QPointF(4.2, 17.8), QPointF(19.8, 17.8));

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(9.1, 6.2), 2.2, 2.2);
        painter.drawEllipse(QPointF(15.2, 12.0), 2.2, 2.2);
        painter.drawEllipse(QPointF(7.1, 17.8), 2.2, 2.2);
        break;
    }
    case Type::Search: {
        painter.drawEllipse(QPointF(10.6, 10.6), 6.2, 6.2);
        painter.drawLine(QPointF(15.3, 15.3), QPointF(20.3, 20.3));
        break;
    }
    case Type::Close: {
        painter.drawLine(QPointF(6.2, 6.2), QPointF(17.8, 17.8));
        painter.drawLine(QPointF(17.8, 6.2), QPointF(6.2, 17.8));
        break;
    }
    case Type::Minimize:
        painter.drawLine(QPointF(5.6, 17.0), QPointF(18.4, 17.0));
        break;
    case Type::Maximize:
        painter.drawRoundedRect(QRectF(5.7, 5.7, 12.6, 12.6), 2.0, 2.0);
        break;
    case Type::Restore: {
        painter.drawRoundedRect(QRectF(7.7, 4.7, 11.6, 11.6), 1.8, 1.8);
        QPainterPath back;
        back.moveTo(5.2, 8.0);
        back.lineTo(4.7, 8.0);
        back.lineTo(4.7, 19.3);
        back.lineTo(16.0, 19.3);
        back.lineTo(16.0, 18.8);
        painter.drawPath(back);
        break;
    }
    case Type::ChevronRight: {
        QPainterPath path;
        path.moveTo(9.3, 5.8);
        path.lineTo(15.5, 12.0);
        path.lineTo(9.3, 18.2);
        painter.drawPath(path);
        break;
    }
    case Type::ChevronLeft: {
        QPainterPath path;
        path.moveTo(14.7, 5.8);
        path.lineTo(8.5, 12.0);
        path.lineTo(14.7, 18.2);
        painter.drawPath(path);
        break;
    }
    case Type::ChevronDown: {
        QPainterPath path;
        path.moveTo(5.8, 9.3);
        path.lineTo(12.0, 15.5);
        path.lineTo(18.2, 9.3);
        painter.drawPath(path);
        break;
    }
    case Type::Play: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPainterPath path;
        path.moveTo(8.3, 5.2);
        path.lineTo(19.0, 12.0);
        path.lineTo(8.3, 18.8);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case Type::Folder: {
        QPainterPath path;
        path.moveTo(3.4, 7.0);
        path.lineTo(9.3, 7.0);
        path.lineTo(11.0, 9.1);
        path.lineTo(20.6, 9.1);
        path.lineTo(20.6, 18.6);
        path.lineTo(3.4, 18.6);
        path.closeSubpath();
        painter.drawPath(path);
        painter.drawLine(QPointF(3.4, 10.6), QPointF(20.6, 10.6));
        break;
    }
    case Type::Check: {
        QPainterPath path;
        path.moveTo(4.9, 12.4);
        path.lineTo(9.8, 17.3);
        path.lineTo(19.1, 6.9);
        painter.drawPath(path);
        break;
    }
    case Type::Sparkles: {
        drawStar(painter, QPointF(11.2, 10.5), 6.0);
        drawStar(painter, QPointF(18.6, 5.5), 2.6);
        drawStar(painter, QPointF(5.0, 18.1), 1.9);
        break;
    }
    case Type::Clock: {
        painter.drawEllipse(QPointF(12.0, 12.0), 8.2, 8.2);
        painter.drawLine(QPointF(12.0, 7.1), QPointF(12.0, 12.0));
        painter.drawLine(QPointF(12.0, 12.0), QPointF(15.5, 13.8));
        break;
    }
    case Type::Cpu: {
        painter.drawRoundedRect(QRectF(6.0, 6.0, 12.0, 12.0), 2.0, 2.0);
        painter.drawRoundedRect(QRectF(9.1, 9.1, 5.8, 5.8), 1.0, 1.0);
        const qreal xs[] = {8.0, 12.0, 16.0};
        for (qreal x : xs) {
            painter.drawLine(QPointF(x, 2.9), QPointF(x, 6.0));
            painter.drawLine(QPointF(x, 18.0), QPointF(x, 21.1));
        }
        const qreal ys[] = {8.0, 12.0, 16.0};
        for (qreal y : ys) {
            painter.drawLine(QPointF(2.9, y), QPointF(6.0, y));
            painter.drawLine(QPointF(18.0, y), QPointF(21.1, y));
        }
        break;
    }
    case Type::Memory: {
        painter.drawRoundedRect(QRectF(2.8, 7.1, 18.4, 9.8), 2.0, 2.0);
        painter.drawLine(QPointF(6.0, 7.1), QPointF(6.0, 12.4));
        painter.drawLine(QPointF(9.0, 7.1), QPointF(9.0, 12.4));
        painter.drawLine(QPointF(12.0, 7.1), QPointF(12.0, 12.4));
        painter.drawLine(QPointF(15.0, 7.1), QPointF(15.0, 12.4));
        painter.drawLine(QPointF(18.0, 7.1), QPointF(18.0, 12.4));
        painter.drawLine(QPointF(5.5, 16.9), QPointF(5.5, 19.6));
        painter.drawLine(QPointF(18.5, 16.9), QPointF(18.5, 19.6));
        break;
    }
    case Type::Palette: {
        QPainterPath path;
        path.moveTo(12.0, 3.4);
        path.cubicTo(17.4, 3.4, 20.7, 7.0, 20.7, 11.2);
        path.cubicTo(20.7, 15.3, 17.8, 17.8, 15.1, 17.8);
        path.lineTo(13.1, 17.8);
        path.cubicTo(11.8, 17.8, 11.3, 19.1, 11.9, 20.0);
        path.cubicTo(12.5, 21.0, 11.7, 21.5, 10.6, 21.5);
        path.cubicTo(6.3, 21.0, 3.3, 17.0, 3.3, 12.2);
        path.cubicTo(3.3, 7.1, 7.2, 3.4, 12.0, 3.4);
        path.closeSubpath();
        painter.drawPath(path);
        const QPointF dots[] = {{7.1, 10.0}, {10.0, 7.1}, {15.1, 7.4}, {17.3, 11.7}};
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        for (const QPointF &dot : dots) {
            painter.drawEllipse(dot, 1.15, 1.15);
        }
        break;
    }
    case Type::Info:
        painter.drawEllipse(QPointF(12.0, 12.0), 8.2, 8.2);
        painter.drawLine(QPointF(12.0, 10.6), QPointF(12.0, 16.4));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(12.0, 7.7), 1.1, 1.1);
        break;
    case Type::Refresh: {
        QPainterPath path;
        path.arcMoveTo(QRectF(4.6, 4.6, 14.8, 14.8), 43.0);
        path.arcTo(QRectF(4.6, 4.6, 14.8, 14.8), 43.0, 278.0);
        painter.drawPath(path);
        QPainterPath arrow;
        arrow.moveTo(16.6, 3.9);
        arrow.lineTo(19.8, 5.1);
        arrow.lineTo(18.1, 8.3);
        painter.drawPath(arrow);
        break;
    }
    case Type::Shield: {
        QPainterPath path;
        path.moveTo(12.0, 3.2);
        path.lineTo(19.5, 6.0);
        path.lineTo(19.5, 11.2);
        path.cubicTo(19.5, 16.1, 16.5, 19.4, 12.0, 21.0);
        path.cubicTo(7.5, 19.4, 4.5, 16.1, 4.5, 11.2);
        path.lineTo(4.5, 6.0);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case Type::Grid:
        painter.drawRoundedRect(QRectF(3.8, 3.8, 6.7, 6.7), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(13.5, 3.8, 6.7, 6.7), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(3.8, 13.5, 6.7, 6.7), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(13.5, 13.5, 6.7, 6.7), 1.5, 1.5);
        break;
    case Type::Download: {
        painter.drawLine(QPointF(12.0, 3.8), QPointF(12.0, 15.3));
        QPainterPath arrow;
        arrow.moveTo(6.8, 10.3);
        arrow.lineTo(12.0, 15.7);
        arrow.lineTo(17.2, 10.3);
        painter.drawPath(arrow);
        QPainterPath tray;
        tray.moveTo(4.8, 16.4);
        tray.lineTo(4.8, 20.1);
        tray.lineTo(19.2, 20.1);
        tray.lineTo(19.2, 16.4);
        painter.drawPath(tray);
        break;
    }
    case Type::External: {
        painter.drawLine(QPointF(13.2, 4.1), QPointF(20.0, 4.1));
        painter.drawLine(QPointF(20.0, 4.1), QPointF(20.0, 10.9));
        painter.drawLine(QPointF(11.1, 13.0), QPointF(19.4, 4.7));
        QPainterPath frame;
        frame.moveTo(16.8, 14.3);
        frame.lineTo(16.8, 19.5);
        frame.lineTo(4.5, 19.5);
        frame.lineTo(4.5, 7.2);
        frame.lineTo(9.7, 7.2);
        painter.drawPath(frame);
        break;
    }
    }

    painter.restore();
}

} // namespace Icons
