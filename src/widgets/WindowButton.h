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

class QEnterEvent;
class QPropertyAnimation;

class WindowButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit WindowButton(Icons::Type icon, bool danger = false, QWidget *parent = nullptr);

    void setIconType(Icons::Type type);
    Icons::Type iconType() const;

    void setDanger(bool danger);
    bool isDanger() const;

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateHover(qreal target);

    Icons::Type m_icon;
    bool m_danger = false;
    qreal m_hoverProgress = 0.0;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
