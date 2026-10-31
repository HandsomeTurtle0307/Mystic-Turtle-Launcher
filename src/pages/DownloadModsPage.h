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
#include <QWidget>

class QGridLayout;
class QLabel;
class SearchField;

class DownloadModsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadModsPage(QWidget *parent = nullptr);

signals:
    void notifyRequested(const QString &message);

private:
    void refreshCards();

    SearchField *m_search = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QString m_category = QStringLiteral("全部");
};
