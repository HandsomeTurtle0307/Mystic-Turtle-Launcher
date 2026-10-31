/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QString>

class QWidget;

class JavaRuntimeInstaller
{
public:
    // Downloads an Adoptium Temurin JDK for the requested major version,
    // extracts it under <gameDir>/runtime and stores the java path in settings.
    static bool download(const QString &gameDirectory,
                         int majorVersion,
                         QWidget *parent,
                         QString *javaPath,
                         QString *errorMessage = nullptr);
};
