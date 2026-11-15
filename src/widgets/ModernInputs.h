/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 */

#pragma once

#include <QComboBox>
#include <QLineEdit>

class QVariantAnimation;

class ModernLineEdit : public QLineEdit
{
public:
    explicit ModernLineEdit(QWidget *parent = nullptr);
    ModernLineEdit(const QString &text, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateProgress(qreal target);
    qreal targetProgress() const;

    QVariantAnimation *m_animation = nullptr;
    qreal m_progress = 0.0;
};

class ModernComboBox : public QComboBox
{
public:
    explicit ModernComboBox(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showPopup() override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateProgress(qreal target);
    qreal targetProgress() const;

    QVariantAnimation *m_animation = nullptr;
    qreal m_progress = 0.0;
};
