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
#include <QWidget>

class QParallelAnimationGroup;

class AnimatedStackedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AnimatedStackedWidget(QWidget *parent = nullptr);

    void addPage(QWidget *page);
    void setCurrentIndex(int index, bool animated = true);
    int currentIndex() const;
    QWidget *currentPage() const;
    int count() const;

    void setAnimationsEnabled(bool enabled);
    bool animationsEnabled() const;

signals:
    void currentChanged(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void animateTo(int index, bool animated);
    void finishTransition();
    void cancelTransition();

    QList<QWidget *> m_pages;
    int m_currentIndex = -1;
    bool m_animationsEnabled = true;
    QParallelAnimationGroup *m_animation = nullptr;
    QWidget *m_outgoing = nullptr;
    QWidget *m_incoming = nullptr;
};
