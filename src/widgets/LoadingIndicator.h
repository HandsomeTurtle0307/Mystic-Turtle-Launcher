/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 */

#pragma once

#include <QColor>
#include <QWidget>

class QHideEvent;
class QShowEvent;
class QTimer;

class LoadingIndicator : public QWidget
{
public:
    explicit LoadingIndicator(QWidget *parent = nullptr,
                              const QColor &color = QColor(110, 231, 183));

    void setColor(const QColor &color);
    void setAnimationsEnabled(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QTimer *m_timer = nullptr;
    QColor m_color;
    int m_angle = 0;
    bool m_animationsEnabled = true;
};
