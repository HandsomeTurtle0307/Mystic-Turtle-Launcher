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

#include <QColor>
#include <QPushButton>

class QEnterEvent;
class QPropertyAnimation;

class AccentButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)

public:
    enum class Variant {
        Primary,
        Secondary,
        Ghost,
        Danger
    };
    Q_ENUM(Variant)

    explicit AccentButton(const QString &text = QString(), QWidget *parent = nullptr);
    explicit AccentButton(const QString &text, Icons::Type icon, QWidget *parent = nullptr);

    void setVariant(Variant variant);
    Variant variant() const;

    void setIconType(Icons::Type type);
    void clearIcon();
    bool hasIcon() const;

    void setAccentColor(const QColor &color);
    QColor accentColor() const;

    void setCompact(bool compact);
    bool isCompact() const;

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);
    qreal pressProgress() const;
    void setPressProgress(qreal progress);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateHover(qreal target);
    void animatePress(qreal target);
    QFont paintFont() const;

    Variant m_variant = Variant::Secondary;
    Icons::Type m_iconType = Icons::Type::ChevronRight;
    bool m_hasIcon = false;
    bool m_compact = false;
    QColor m_accent = QColor(QStringLiteral("#6EE7B7"));
    qreal m_hoverProgress = 0.0;
    qreal m_pressProgress = 0.0;
    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_pressAnimation = nullptr;
};
