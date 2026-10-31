/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "mainwindow.h"
#include "Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLocale>

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    Theme::apply(app);

    MainWindow window;
    window.show();

    return QApplication::exec();
}
