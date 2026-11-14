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
#include <QFont>
#include <QString>

class QApplication;
class QFrame;
class QLabel;
class QProgressBar;
class QWidget;

namespace Theme {

inline QColor BackgroundDeep(QStringLiteral("#080B10"));
inline QColor Background(QStringLiteral("#0D1117"));
inline QColor Surface(QStringLiteral("#111821"));
inline QColor SurfaceRaised(QStringLiteral("#17212C"));
inline QColor SurfaceSoft(QStringLiteral("#1B2530"));
inline QColor TextPrimary(QStringLiteral("#F1F6F3"));
inline QColor TextSecondary(QStringLiteral("#A4B1B9"));
inline QColor TextMuted(QStringLiteral("#70808B"));
inline QColor Accent(QStringLiteral("#6EE7B7"));
inline QColor AccentStrong(QStringLiteral("#34D399"));
inline QColor AccentDark(QStringLiteral("#07241B"));
inline QColor Blue(QStringLiteral("#89A8FF"));
inline QColor Purple(QStringLiteral("#B7A0FF"));
inline QColor Amber(QStringLiteral("#FFC66D"));
inline QColor Danger(QStringLiteral("#FF6B78"));
inline QColor Border(255, 255, 255, 22);

QFont font(int pixelSize, QFont::Weight weight = QFont::Normal, bool display = false);

QLabel *label(const QString &text,
              int pixelSize,
              QFont::Weight weight = QFont::Normal,
              const QColor &color = TextPrimary,
              QWidget *parent = nullptr,
              bool display = false);

QLabel *badge(const QString &text,
              const QColor &foreground,
              const QColor &background,
              QWidget *parent = nullptr,
              const QColor &border = Qt::transparent);

QFrame *divider(QWidget *parent = nullptr);

inline bool isDarkMode()
{
    return true;
}

void apply(QApplication &app);
bool animationsEnabled();
void setAnimationsEnabled(bool enabled);
bool liquidGlassEnabled();
void setLiquidGlassEnabled(bool enabled);
bool buttonSpotlightEnabled();
void setButtonSpotlightEnabled(bool enabled);
void setProgressValue(QProgressBar *progressBar, int value);

} // namespace Theme
