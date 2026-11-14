/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPointF>
#include <QWidget>

class QMouseEvent;
class QTimer;

class AnimePortraitWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AnimePortraitWidget(QWidget *parent = nullptr);

    void setActive(bool active);
    void setAnimationsEnabled(bool enabled);
    void setPortrait(const QString &resourceName);
    void setMotionParameters(int breathPercent, int swayPercent, int eyePercent);
    bool isActive() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    struct EyeInfo
    {
        bool enabled = false;
        QPointF left;
        QPointF right;
        QPointF radius{0.05, 0.025};
    };

    void loadImage();
    void loadEyeInfo();
    void buildHairLayer();
    void tick();
    void updateCursorTarget();

    QImage m_fullImage;
    QImage m_hairImage;
    QString m_resourceName = QStringLiteral("portrait-maid.png");
    EyeInfo m_eyeInfo;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    QPointF m_look;
    QPointF m_targetLook;
    qreal m_time = 0.0;
    qreal m_breathAmplitude = 0.55;
    qreal m_swayAmplitude = 0.55;
    qreal m_eyeAmplitude = 0.60;
    bool m_active = false;
    bool m_animationsEnabled = true;
};
