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

class AvatarWidget : public QWidget
{
public:
    explicit AvatarWidget(int diameter = 88, QWidget *parent = nullptr);

    void setOnline(bool online);
    bool isOnline() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_diameter;
    bool m_online = true;
};
