/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QImage>
#include <QObject>

// Single source of truth for the launcher's background image. Child windows
// listen to changed() so they cannot keep painting a stale copy after the
// user picks or clears a background.
class BackgroundStore : public QObject
{
    Q_OBJECT

public:
    static BackgroundStore &instance();

    void setBackground(const QImage &image, int opacity);
    void clear();

    QImage image() const;
    int opacity() const;

signals:
    void changed();

private:
    explicit BackgroundStore(QObject *parent = nullptr);

    QImage m_image;
    int m_opacity = 35;
};
