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
#include <QRectF>
#include <QWidget>

class NavButton;
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

private:
    QRectF targetIndicatorRect(int index) const;
    void updateIndicator(bool animated);

    QList<NavButton *> m_buttons;
    int m_currentIndex = 0;
    QRectF m_indicatorRect;
    QRectF m_indicatorFrom;
    QRectF m_indicatorTo;
    bool m_indicatorInitialized = false;
    bool m_animationsEnabled = true;
    QVariantAnimation *m_indicatorAnimation = nullptr;
};
