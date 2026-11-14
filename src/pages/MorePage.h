/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QWidget>

class AnimatedStackedWidget;

class MorePage : public QWidget
{
    Q_OBJECT

public:
    explicit MorePage(QWidget *parent = nullptr);

    void setAnimationsEnabled(bool enabled);
    void showOverview();

signals:
    void downloadModpacksRequested();

private:
    AnimatedStackedWidget *m_pages = nullptr;
    bool m_animationsEnabled = true;
};
