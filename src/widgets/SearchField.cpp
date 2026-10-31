/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "SearchField.h"

#include "IconFactory.h"
#include "Theme.h"

SearchField::SearchField(const QString &placeholder, QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(placeholder);
    setClearButtonEnabled(true);
    setFixedHeight(42);
    setFont(Theme::font(13));
    setMinimumWidth(200);

    addAction(Icons::icon(Icons::Type::Search, Theme::TextMuted, 16),
              QLineEdit::LeadingPosition);
}
