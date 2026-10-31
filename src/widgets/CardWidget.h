/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QColor>
#include <QWidget>

class QEnterEvent;
class QPropertyAnimation;

class CardWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit CardWidget(QWidget *parent = nullptr);

    void setAccentColor(const QColor &color);
    QColor accentColor() const;

    void setInteractive(bool interactive);
    bool isInteractive() const;

    void setCornerRadius(qreal radius);
    qreal cornerRadius() const;

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateHover(qreal target);

    QColor m_accent = QColor(QStringLiteral("#6EE7B7"));
    qreal m_hoverProgress = 0.0;
    qreal m_cornerRadius = 18.0;
    bool m_interactive = true;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
