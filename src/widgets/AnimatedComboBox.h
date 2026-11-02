/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License.
 */

#pragma once

#include <QComboBox>
#include <QPointer>

class QPropertyAnimation;

class AnimatedComboBox : public QComboBox
{
public:
    explicit AnimatedComboBox(QWidget *parent = nullptr);

protected:
    void showPopup() override;
    void hidePopup() override;

private:
    QPointer<QPropertyAnimation> m_expandAnimation;
};
