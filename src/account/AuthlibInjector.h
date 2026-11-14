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

#include <functional>

class AuthlibInjector
{
public:
    // Returns a usable authlib-injector.jar path, downloading it if needed.
    static QString ensureJar(QString *errorMessage,
                             const std::function<bool()> &cancelled = {});

    static QString cachedJarPath();
};
