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

class NavButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal activeProgress READ activeProgress WRITE setActiveProgress)

public:
    NavButton(const QString &text, Icons::Type icon, QWidget *parent = nullptr);

    void setActive(bool active, bool animated = true);
    bool isActive() const;

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);

    qreal activeProgress() const;
    void setActiveProgress(qreal progress);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Icons::Type m_icon;
    qreal m_hoverProgress = 0.0;
    qreal m_activeProgress = 0.0;
    bool m_active = false;
    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_activeAnimation = nullptr;
};
