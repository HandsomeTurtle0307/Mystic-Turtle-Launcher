/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "IconFactory.h"

#include <QAbstractButton>
#include <QPointF>

class QEnterEvent;
class QMouseEvent;
class QPropertyAnimation;

class WindowButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal spotlightProgress READ spotlightProgress WRITE setSpotlightProgress)

public:
    explicit WindowButton(Icons::Type icon, bool danger = false, QWidget *parent = nullptr);

    void setIconType(Icons::Type type);
    Icons::Type iconType() const;

    void setDanger(bool danger);
    bool isDanger() const;

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);
    qreal spotlightProgress() const;
    void setSpotlightProgress(qreal progress);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void animateHover(qreal target);
    void animateSpotlight(qreal target);

    Icons::Type m_icon;
    bool m_danger = false;
    qreal m_hoverProgress = 0.0;
    qreal m_spotlightProgress = 0.0;
    QPointF m_spotlightPosition;
    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_spotlightAnimation = nullptr;
};
