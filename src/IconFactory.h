/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QColor>
#include <QIcon>

class QPainter;
class QRectF;

namespace Icons {

enum class Type {
    Home,
    Gamepad,
    Cube,
    Sliders,
    Search,
    Close,
    Minimize,
    Maximize,
    Restore,
    ChevronRight,
    ChevronLeft,
    ChevronDown,
    Play,
    Folder,
    Check,
    Sparkles,
    Clock,
    Cpu,
    Memory,
    Palette,
    Info,
    Refresh,
    Shield,
    Package,
    Grid,
    Download,
    External
};

QIcon icon(Type type, const QColor &color, int size = 18);
void paint(QPainter &painter,
           Type type,
           const QRectF &rect,
           const QColor &color,
           qreal strokeWidth = 1.7);

} // namespace Icons
