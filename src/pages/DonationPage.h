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

class DonationPage : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal glowProgress READ glowProgress WRITE setGlowProgress)

public:
    explicit DonationPage(QWidget *parent = nullptr);

    void setAnimationsEnabled(bool enabled);
    qreal glowProgress() const;
    void setGlowProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateAnimationState();

    QVariantAnimation *m_glowAnimation = nullptr;
    qreal m_glowProgress = 0.0;
    bool m_animationsEnabled = true;
};
