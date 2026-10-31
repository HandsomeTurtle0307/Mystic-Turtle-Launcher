/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "Theme.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QStringList>
#include <QStyleFactory>

namespace {

// Theme colors are mutable so the light/dark palette can be selected before
// the main window is constructed.
bool g_darkMode = true;

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
    auto *result = label(text, 11, QFont::DemiBold, foreground, parent);
    result->setAlignment(Qt::AlignCenter);
    result->setStyleSheet(QStringLiteral(
                              "background: %1; color: %2; border: 1px solid %3;"
                              "border-radius: 8px; padding: 4px 9px;")
                              .arg(rgba(background), foreground.name(), rgba(border)));
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
