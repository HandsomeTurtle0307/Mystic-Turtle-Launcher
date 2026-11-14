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

class QVariantAnimation;

// Decorative, text-free turtle geometry used by the blank home page.
class TurtleAnimationWidget : public QWidget
{
public:
    explicit TurtleAnimationWidget(QWidget *parent = nullptr);

    void setActive(bool active);
    void setAnimationsEnabled(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateAnimationState();
    void setProgress(qreal progress);

    QVariantAnimation *m_animation = nullptr;
    qreal m_progress = 0.0;
    bool m_active = false;
    bool m_animationsEnabled = true;
};
