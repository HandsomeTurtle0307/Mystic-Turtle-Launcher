/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QWidget>

class NavButton;
class QMouseEvent;
class QVariantAnimation;

class NavigationBar : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationBar(QWidget *parent = nullptr);

    void setCurrentIndex(int index, bool animated = true);
    int currentIndex() const;

    void setItemEnabled(int index, bool enabled);
    bool isItemEnabled(int index) const;

    void setAnimationsEnabled(bool enabled);
    bool animationsEnabled() const;

signals:
    void navigationRequested(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QRectF targetIndicatorRect(int index) const;
    void updateIndicator(bool animated);
    void updateSpotlightPosition(const QPoint &globalPosition);
    void animateSpotlight(qreal target);

    QList<NavButton *> m_buttons;
    int m_currentIndex = 0;
    QRectF m_indicatorRect;
    QRectF m_indicatorFrom;
    QRectF m_indicatorTo;
    bool m_indicatorInitialized = false;
    bool m_animationsEnabled = true;
    qreal m_spotlightProgress = 0.0;
    QPointF m_spotlightPosition;
    QVariantAnimation *m_indicatorAnimation = nullptr;
    QVariantAnimation *m_spotlightAnimation = nullptr;
};
