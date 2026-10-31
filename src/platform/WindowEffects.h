/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

class QWidget;

namespace WindowEffects {

bool isAvailable();

// Applies DWM's Windows 11 backdrop (Acrylic-like transient material) and,
// where unavailable, falls back to the legacy accent blur behind the window.
void apply(QWidget *window, bool acrylic, bool roundedCorners);

void setAcrylic(QWidget *window, bool enabled);
void setRoundedCorners(QWidget *window, bool rounded);

} // namespace WindowEffects
