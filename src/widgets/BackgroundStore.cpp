/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "BackgroundStore.h"

#include <QtGlobal>

BackgroundStore::BackgroundStore(QObject *parent)
    : QObject(parent)
{
}

BackgroundStore &BackgroundStore::instance()
{
    static BackgroundStore store;
    return store;
}

void BackgroundStore::setBackground(const QImage &image, int opacity)
{
    m_image = image;
    m_opacity = qBound(0, opacity, 100);
    emit changed();
}

void BackgroundStore::clear()
{
    m_image = QImage();
    m_opacity = 35;
    emit changed();
}

QImage BackgroundStore::image() const
{
    return m_image;
}

int BackgroundStore::opacity() const
{
    return m_opacity;
}
