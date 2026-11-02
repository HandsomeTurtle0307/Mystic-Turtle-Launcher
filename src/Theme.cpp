/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "Theme.h"

#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPointer>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QHash>
#include <QStringList>
#include <QStyleFactory>
#include <QWheelEvent>

namespace {

// Theme colors are mutable so the light/dark palette can be selected before
// the main window is constructed.
bool g_darkMode = true;
bool g_animationsEnabled = true;
QHash<QProgressBar *, QPointer<QPropertyAnimation>> g_progressAnimations;
QPointer<QObject> g_smoothScrollFilter;

class SmoothScrollFilter final : public QObject
{
public:
    using QObject::QObject;

    void resetButtonEffects()
    {
        for (auto it = m_buttonEffects.cbegin(); it != m_buttonEffects.cend(); ++it) {
            if (!it.key()) {
                continue;
            }
            if (it.value()) {
                it.value()->setOpacity(1.0);
            }
        }
        m_buttonAnimations.clear();
        m_buttonEffects.clear();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (auto *button = qobject_cast<QAbstractButton *>(watched)) {
            if (event->type() == QEvent::MouseButtonPress
                || event->type() == QEvent::MouseButtonDblClick) {
                animateButton(button, 0.78, 70);
            } else if (event->type() == QEvent::MouseButtonRelease) {
                animateButton(button, 1.0, 150);
            } else if (event->type() == QEvent::KeyPress) {
                const auto *key = static_cast<QKeyEvent *>(event);
                if (!key->isAutoRepeat()
                    && (key->key() == Qt::Key_Space || key->key() == Qt::Key_Return
                        || key->key() == Qt::Key_Enter)) {
                    animateButton(button, 0.78, 70);
                }
            } else if (event->type() == QEvent::KeyRelease) {
                const auto *key = static_cast<QKeyEvent *>(event);
                if (!key->isAutoRepeat()
                    && (key->key() == Qt::Key_Space || key->key() == Qt::Key_Return
                        || key->key() == Qt::Key_Enter)) {
                    animateButton(button, 1.0, 150);
                }
            }
        }

        if (event->type() != QEvent::Wheel) {
            return QObject::eventFilter(watched, event);
        }

        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            return false;
        }

        auto *widget = qobject_cast<QWidget *>(watched);
        QAbstractScrollArea *area = nullptr;
        for (QWidget *parent = widget; parent; parent = parent->parentWidget()) {
            area = qobject_cast<QAbstractScrollArea *>(parent);
            if (area) {
                break;
            }
        }
        if (!area) {
            return false;
        }

        const QPoint pixelDelta = wheel->pixelDelta();
        const QPoint angleDelta = wheel->angleDelta();
        const bool horizontal = qAbs(angleDelta.x()) > qAbs(angleDelta.y())
            || (angleDelta.y() == 0 && pixelDelta.x() != 0);
        QScrollBar *scrollBar = horizontal ? area->horizontalScrollBar()
                                           : area->verticalScrollBar();
        const int wheelDelta = horizontal ? angleDelta.x() : angleDelta.y();
        const int pixelMovement = horizontal ? pixelDelta.x() : pixelDelta.y();
        const qreal movement = pixelMovement != 0
            ? -pixelMovement
            : -wheelDelta / 120.0 * scrollBar->singleStep() * 3.0;
        if (qFuzzyIsNull(movement) || scrollBar->maximum() <= scrollBar->minimum()) {
            return false;
        }

        QPointer<QPropertyAnimation> &current = m_animations[scrollBar];
        const int currentTarget = current
            ? current->endValue().toInt()
            : scrollBar->value();
        const int target = qBound(scrollBar->minimum(),
                                  qRound(currentTarget + movement),
                                  scrollBar->maximum());
        if (target == currentTarget) {
            return false;
        }

        if (current) {
            current->stop();
            current->deleteLater();
        }

        auto *animation = new QPropertyAnimation(scrollBar, "value", scrollBar);
        current = animation;
        animation->setDuration(210);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(scrollBar->value());
        animation->setEndValue(target);
        QObject::connect(animation, &QPropertyAnimation::finished, scrollBar,
                         [this, scrollBar, animation]() {
            if (m_animations.value(scrollBar) == animation) {
                m_animations.remove(scrollBar);
            }
            animation->deleteLater();
        });
        QObject::connect(scrollBar, &QObject::destroyed, animation,
                         [this, scrollBar]() { m_animations.remove(scrollBar); });
        animation->start();
        wheel->accept();
        return true;
    }

private:
    void animateButton(QAbstractButton *button, qreal opacity, int duration)
    {
        if (!button->isEnabled()) {
            return;
        }

        auto *effect = qobject_cast<QGraphicsOpacityEffect *>(button->graphicsEffect());
        if (!effect) {
            if (button->graphicsEffect()) {
                return;
            }
            effect = new QGraphicsOpacityEffect(button);
            effect->setOpacity(1.0);
            effect->setProperty("uiPressAnimationEffect", true);
            button->setGraphicsEffect(effect);
            m_buttonEffects.insert(button, effect);
            QObject::connect(button, &QObject::destroyed, this,
                             [this, button]() {
                m_buttonAnimations.remove(button);
                m_buttonEffects.remove(button);
            });
        } else if (!effect->property("uiPressAnimationEffect").toBool()) {
            return;
        }

        QPointer<QPropertyAnimation> &current = m_buttonAnimations[button];
        if (current) {
            current->stop();
            current->deleteLater();
        }

        auto *animation = new QPropertyAnimation(effect, "opacity", this);
        current = animation;
        animation->setDuration(duration);
        animation->setEasingCurve(opacity < effect->opacity()
                                      ? QEasingCurve::InCubic
                                      : QEasingCurve::OutCubic);
        animation->setStartValue(effect->opacity());
        animation->setEndValue(opacity);
        QObject::connect(animation, &QPropertyAnimation::finished, this,
                         [this, button, animation]() {
            if (m_buttonAnimations.value(button) == animation) {
                m_buttonAnimations.remove(button);
            }
            animation->deleteLater();
        });
        animation->start();
    }

    QHash<QScrollBar *, QPointer<QPropertyAnimation>> m_animations;
    QHash<QAbstractButton *, QPointer<QPropertyAnimation>> m_buttonAnimations;
    QHash<QAbstractButton *, QPointer<QGraphicsOpacityEffect>> m_buttonEffects;
};

QString rgba(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString controlStyleSheet()
{
    if (g_darkMode) {
        return QStringLiteral(R"(
            QToolTip {
                color: #EAF1F7;
                background-color: #16202C;
                border: 1px solid rgba(255, 255, 255, 34);
                border-radius: 8px;
                padding: 6px 9px;
                font-size: 12px;
            }
            QLineEdit {
                color: #EAF1F7;
                background-color: rgba(255, 255, 255, 9);
                border: 1px solid rgba(255, 255, 255, 24);
                border-radius: 10px;
                padding: 8px 12px;
                selection-background-color: rgba(110, 231, 183, 110);
                selection-color: #FFFFFF;
            }
            QLineEdit:hover {
                border-color: rgba(255, 255, 255, 42);
                background-color: rgba(255, 255, 255, 13);
            }
            QLineEdit:focus {
                border-color: #6EE7B7;
                background-color: rgba(255, 255, 255, 16);
            }
            QLineEdit:disabled {
                color: #6F7D8C;
                background-color: rgba(255, 255, 255, 5);
            }
            QComboBox {
                color: #EAF1F7;
                background-color: #16202C;
                border: 1px solid rgba(255, 255, 255, 26);
                border-radius: 9px;
                padding: 6px 10px;
            }
            QComboBox QAbstractItemView {
                color: #EAF1F7;
                background: #16202C;
                border: 1px solid rgba(255, 255, 255, 28);
                selection-background-color: #24364D;
            }
            QScrollArea { border: none; background: transparent; }
            QScrollArea > QWidget > QWidget { background: transparent; }
            QScrollBar:vertical {
                background: transparent; width: 9px; margin: 2px 1px;
            }
            QScrollBar::handle:vertical {
                background: rgba(255, 255, 255, 40); min-height: 30px; border-radius: 4px;
            }
            QScrollBar::handle:vertical:hover { background: rgba(255, 255, 255, 70); }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px; background: transparent;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: transparent;
            }
            QScrollBar:horizontal {
                background: transparent; height: 9px; margin: 1px 2px;
            }
            QScrollBar::handle:horizontal {
                background: rgba(255, 255, 255, 40); min-width: 30px; border-radius: 4px;
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                width: 0px; background: transparent;
            }
            QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
                background: transparent;
            }
            QSlider::groove:horizontal { height: 4px; background: #263545; border-radius: 2px; }
            QSlider::sub-page:horizontal { background: #6EE7B7; border-radius: 2px; }
            QSlider::add-page:horizontal { background: #263545; border-radius: 2px; }
            QSlider::handle:horizontal {
                width: 15px; height: 15px; margin: -6px 0; border-radius: 7px;
                background: #EAF1F7; border: 3px solid #6EE7B7;
            }
            QProgressBar {
                min-height: 6px; max-height: 6px; border: none; border-radius: 3px;
                background: rgba(255, 255, 255, 22); text-align: center; color: transparent;
            }
            QProgressBar::chunk { border-radius: 3px; background: #6EE7B7; }
            QFrame[role="modRow"] { border-radius: 12px; background: transparent; }
            QFrame[role="modRow"]:hover { background: rgba(110, 231, 183, 18); }
            QFileDialog, QMessageBox, QInputDialog, QProgressDialog {
                background-color: #111923; color: #EAF1F7;
            }
        )");
    }

    return QStringLiteral(R"(
        QToolTip {
            color: #17243A;
            background-color: #FFFFFF;
            border: 1px solid #D7E3F2;
            border-radius: 8px;
            padding: 6px 9px;
            font-size: 12px;
        }

        QLineEdit {
            color: #17243A;
            background-color: #FFFFFF;
            border: 1px solid #D7E3F2;
            border-radius: 10px;
            padding: 8px 12px;
            selection-background-color: #CFE2FF;
            selection-color: #17243A;
        }
        QLineEdit:hover {
            border-color: #B9CCE3;
            background-color: #FBFDFF;
        }
        QLineEdit:focus {
            border-color: #2F80ED;
            background-color: #FFFFFF;
        }
        QLineEdit:disabled {
            color: #9AA8B8;
            background-color: #F3F7FC;
        }

        QComboBox {
            color: #17243A;
            background-color: #FFFFFF;
            border: 1px solid #D7E3F2;
            border-radius: 9px;
            padding: 6px 10px;
        }
        QComboBox QAbstractItemView {
            color: #17243A;
            background: #FFFFFF;
            border: 1px solid #D7E3F2;
            selection-background-color: #E8F2FF;
            selection-color: #1E6FE0;
        }

        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 2px 1px 2px 1px;
        }
        QScrollBar::handle:vertical {
            background: rgba(23, 36, 58, 42);
            min-height: 30px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(23, 36, 58, 72);
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 9px;
            margin: 1px 2px 1px 2px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(23, 36, 58, 42);
            min-width: 30px;
            border-radius: 4px;
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0px;
            background: transparent;
        }
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }

        QSlider::groove:horizontal {
            height: 4px;
            background: #E1EBF7;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #2F80ED;
            border-radius: 2px;
        }
        QSlider::add-page:horizontal {
            background: #E1EBF7;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 15px;
            height: 15px;
            margin: -6px 0;
            border-radius: 7px;
            background: #FFFFFF;
            border: 3px solid #2F80ED;
        }
        QSlider::handle:horizontal:hover {
            background: #EAF3FF;
            border-color: #1E6FE0;
        }
        QSlider::handle:horizontal:pressed {
            background: #2F80ED;
            border-color: #1E6FE0;
        }

        QProgressBar {
            min-height: 6px;
            max-height: 6px;
            border: none;
            border-radius: 3px;
            background: #E4EDF9;
            text-align: center;
            color: transparent;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background: #2F80ED;
        }

        QFrame[role="modRow"] {
            border-radius: 12px;
            background: transparent;
        }
        QFrame[role="modRow"]:hover {
            background: rgba(47, 128, 237, 12);
        }

        QFileDialog, QMessageBox, QInputDialog, QProgressDialog {
            background-color: #FFFFFF;
            color: #17243A;
        }
    )");
}

} // namespace

namespace Theme {

bool animationsEnabled()
{
    return g_animationsEnabled;
}

void setAnimationsEnabled(bool enabled)
{
    g_animationsEnabled = enabled;
    auto *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) {
        return;
    }

    if (enabled) {
        if (!g_smoothScrollFilter) {
            g_smoothScrollFilter = new SmoothScrollFilter(app);
            app->installEventFilter(g_smoothScrollFilter);
        }
        return;
    }

    auto *filter = dynamic_cast<SmoothScrollFilter *>(g_smoothScrollFilter.data());
    if (g_smoothScrollFilter) {
        app->removeEventFilter(g_smoothScrollFilter);
        g_smoothScrollFilter = nullptr;
    }

    for (QAbstractAnimation *animation : app->findChildren<QAbstractAnimation *>()) {
        if (animation->state() == QAbstractAnimation::Running
            && animation->duration() >= 0) {
            animation->setCurrentTime(animation->duration());
            animation->stop();
        }
    }
    if (filter) {
        filter->resetButtonEffects();
        filter->deleteLater();
    }
}

void setProgressValue(QProgressBar *progressBar, int value)
{
    if (!progressBar) {
        return;
    }

    QPointer<QPropertyAnimation> &current = g_progressAnimations[progressBar];
    if (!animationsEnabled() || progressBar->minimum() == progressBar->maximum()) {
        if (current) {
            current->stop();
            current->deleteLater();
            current = nullptr;
        }
        progressBar->setValue(value);
        return;
    }

    if (current) {
        current->stop();
        current->deleteLater();
    }
    auto *animation = new QPropertyAnimation(progressBar, "value", progressBar);
    current = animation;
    animation->setDuration(180);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(progressBar->value());
    animation->setEndValue(value);
    QObject::connect(animation, &QPropertyAnimation::finished, progressBar,
                     [progressBar, animation]() {
        if (g_progressAnimations.value(progressBar) == animation) {
            g_progressAnimations.remove(progressBar);
        }
        animation->deleteLater();
    });
    QObject::connect(progressBar, &QObject::destroyed, animation,
                     [progressBar]() { g_progressAnimations.remove(progressBar); });
    animation->start();
}

QFont font(int pixelSize, QFont::Weight weight, bool display)
{
    QFont result;
    result.setFamilies(display
                           ? QStringList{QStringLiteral("Segoe UI Variable Display"),
                                         QStringLiteral("Microsoft YaHei UI"),
                                         QStringLiteral("Segoe UI"),
                                         QStringLiteral("Noto Sans CJK SC")}
                           : QStringList{QStringLiteral("Segoe UI Variable Text"),
                                         QStringLiteral("Microsoft YaHei UI"),
                                         QStringLiteral("Segoe UI"),
                                         QStringLiteral("Noto Sans CJK SC")});
    result.setPixelSize(pixelSize);
    result.setWeight(weight);
    result.setStyleStrategy(QFont::PreferAntialias);
    return result;
}

QLabel *label(const QString &text,
              int pixelSize,
              QFont::Weight weight,
              const QColor &color,
              QWidget *parent,
              bool display)
{
    auto *result = new QLabel(text, parent);
    result->setFont(font(pixelSize, weight, display));
    result->setAttribute(Qt::WA_TransparentForMouseEvents);
    QPalette palette = result->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::Text, color);
    result->setPalette(palette);
    result->setStyleSheet(QStringLiteral("background: transparent;"));
    return result;
}

QLabel *badge(const QString &text,
              const QColor &foreground,
              const QColor &background,
              QWidget *parent,
              const QColor &border)
{
    Q_UNUSED(background)
    Q_UNUSED(border)
    auto *result = label(text, 11, QFont::DemiBold, foreground, parent);
    result->setAlignment(Qt::AlignCenter);
    result->setStyleSheet(QStringLiteral(
                              "background: transparent; color: %1; border: none;"
                              "padding: 4px 9px;")
                              .arg(foreground.name()));
    return result;
}

QFrame *divider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    line->setAttribute(Qt::WA_StyledBackground, true);
    line->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(rgba(Border)));
    return line;
}

void apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setFont(font(13));
    app.setStyleSheet(controlStyleSheet());

    QPalette palette;
    palette.setColor(QPalette::Window, Background);
    palette.setColor(QPalette::WindowText, TextPrimary);
    palette.setColor(QPalette::Base, SurfaceRaised);
    palette.setColor(QPalette::AlternateBase, Surface);
    palette.setColor(QPalette::Text, TextPrimary);
    palette.setColor(QPalette::Button, SurfaceRaised);
    palette.setColor(QPalette::ButtonText, TextPrimary);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, AccentStrong);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, SurfaceRaised);
    palette.setColor(QPalette::ToolTipText, TextPrimary);
    palette.setColor(QPalette::PlaceholderText, TextMuted);
    palette.setColor(QPalette::Link, Accent);
    palette.setColor(QPalette::Mid, Border);
    palette.setColor(QPalette::Dark, BackgroundDeep);
    app.setPalette(palette);

    app.setApplicationDisplayName(QStringLiteral("MysticTurtle"));
    app.setApplicationName(QStringLiteral("MysticTurtle"));
    app.setOrganizationName(QStringLiteral("MysticTurtle"));
}

} // namespace Theme
