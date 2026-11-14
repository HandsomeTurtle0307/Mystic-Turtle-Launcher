/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "SampleData.h"

#include <QString>
#include <QVector>
#include <QWidget>

class AccentButton;
class QGridLayout;
class QLabel;
class QScrollArea;
class QWidget;
class VersionCard;

class VersionSelectPage : public QWidget
{
    Q_OBJECT

public:
    explicit VersionSelectPage(QWidget *parent = nullptr);

    void setVersions(const QVector<VersionInfo> &versions, int selectedIndex,
                     const QString &gameDirectory);
    void setSelectedIndex(int selectedIndex);

signals:
    void backRequested();
    void versionSelected(int index);
    void detailsRequested(int index);
    void deleteRequested(int index);

private:
    void rebuildCards();

    QVector<VersionInfo> m_versions;
    QString m_gameDirectory;
    int m_selectedIndex = -1;
    QLabel *m_countLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QWidget *m_cardsContent = nullptr;
    QGridLayout *m_grid = nullptr;
    QScrollArea *m_scroll = nullptr;
    QVector<VersionCard *> m_cards;
};
