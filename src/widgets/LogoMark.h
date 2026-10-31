/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QWidget>

class LogoMark : public QWidget
{
public:
    explicit LogoMark(int size = 36, QWidget *parent = nullptr);

    static QIcon appIcon();

protected:
    void paintEvent(QPaintEvent *event) override;
};
